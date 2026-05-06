#include "service_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <systemd/sd-event.h>
#include "config.h"
#include "event_loop.h"

int parse_sec_to_int(const char *str, int *out);

static ServiceEntry service_table[COREINITD_MAX_SERVICES_CAPACITY];
static size_t service_count = 0;

typedef struct {
    ServiceEntry *entry;
} RestartJob;

static const char *service_state_name(ServiceState state) {
    switch (state) {
        case SERVICE_INACTIVE: return "inactive";
        case SERVICE_STARTING: return "starting";
        case SERVICE_ACTIVE: return "active";
        case SERVICE_STOPPING: return "stopping";
        case SERVICE_FAILED: return "failed";
    }
    return "unknown";
}

static int bounded_strlen(const char *value, size_t capacity, size_t *len) {
    if (!value || capacity == 0)
        return -1;

    const void *terminator = memchr(value, '\0', capacity);
    if (!terminator)
        return -1;

    if (len)
        *len = (const char *)terminator - value;
    return 0;
}

static const char *unit_name_or_unknown(const Unit *unit) {
    if (!unit)
        return "<null-unit>";
    if (bounded_strlen(unit->name, sizeof(unit->name), NULL) != 0)
        return "<unterminated-unit-name>";
    if (unit->name[0] == '\0')
        return "<unnamed-unit>";
    return unit->name;
}

static int unit_field_is_nonempty(const char *field, size_t capacity, const char *field_name, const Unit *unit) {
    size_t len = 0;
    if (bounded_strlen(field, capacity, &len) != 0) {
        fprintf(stderr, "[service_manager] %s for %s is not safely NUL-terminated within %zu bytes\n",
                field_name, unit_name_or_unknown(unit), capacity);
        return 0;
    }

    return len > 0;
}

static int parse_seconds_or_default(const char *value, size_t capacity, int fallback) {
    int seconds;
    size_t len = 0;

    if (bounded_strlen(value, capacity, &len) != 0 || len == 0)
        return fallback;
    if (parse_sec_to_int(value, &seconds) == 0)
        return seconds;
    fprintf(stderr, "[service_manager] Invalid time value '%s', using %ds\n", value, fallback);
    return fallback;
}

static int list_contains_status(const UnitStringList *list, int code) {
    if (!list)
        return 0;

    char needle[32];
    snprintf(needle, sizeof(needle), "%d", code);

    size_t count = list->count;
    if (count > UNIT_LIST_MAX) {
        fprintf(stderr, "[service_manager] Unit string list count %zu exceeds capacity %d; clamping scan\n",
                count, UNIT_LIST_MAX);
        count = UNIT_LIST_MAX;
    }

    for (size_t i = 0; i < count; i++) {
        if (bounded_strlen(list->values[i], sizeof(list->values[i]), NULL) != 0) {
            fprintf(stderr, "[service_manager] Ignoring unterminated status list entry at index %zu\n", i);
            continue;
        }

        char buf[UNIT_VALUE_LEN];
        snprintf(buf, sizeof(buf), "%s", list->values[i]);

        char *saveptr = NULL;
        for (char *tok = strtok_r(buf, " \t,", &saveptr); tok; tok = strtok_r(NULL, " \t,", &saveptr)) {
            if (strcmp(tok, needle) == 0)
                return 1;
        }
    }

    return 0;
}

static int status_is_success(const Unit *unit, int status) {
    if (!unit)
        return 0;

    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        return code == 0 || list_contains_status(&unit->success_exit_status, code);
    }

    if (WIFSIGNALED(status))
        return list_contains_status(&unit->success_exit_status, WTERMSIG(status));

    return 0;
}

static int status_forces_restart(const Unit *unit, int status) {
    if (!unit)
        return 0;

    int code;
    if (WIFEXITED(status))
        code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
        code = WTERMSIG(status);
    else
        return 0;

    return list_contains_status(&unit->restart_force_exit_status, code);
}

static int restart_policy_allows(const Unit *unit, int status) {
    if (!unit)
        return 0;

    size_t restart_len = 0;
    if (bounded_strlen(unit->restart, sizeof(unit->restart), &restart_len) != 0) {
        fprintf(stderr, "[service_manager] Restart policy for %s is not safely NUL-terminated; not restarting\n",
                unit_name_or_unknown(unit));
        return 0;
    }

    const char *policy = restart_len > 0 ? unit->restart : "no";

    if (status_forces_restart(unit, status))
        return 1;

    if (strcasecmp(policy, "no") == 0 || strcasecmp(policy, "false") == 0)
        return 0;
    if (strcasecmp(policy, "always") == 0)
        return 1;
    if (strcasecmp(policy, "on-failure") == 0)
        return !status_is_success(unit, status);
    if (strcasecmp(policy, "on-success") == 0)
        return status_is_success(unit, status);

    fprintf(stderr, "[service_manager] Unsupported Restart=%s for %s; not restarting\n",
            policy, unit_name_or_unknown(unit));
    return 0;
}

static int start_limit_allows(ServiceEntry *entry) {
    if (!entry || !entry->unit) {
        fprintf(stderr, "[service_manager] Cannot evaluate start limit for a NULL service entry\n");
        return 0;
    }

    int burst = entry->unit->start_limit_burst;
    if (burst <= 0)
        return 1;

    if (entry->restart_count < burst)
        return 1;

    fprintf(stderr, "[service_manager] Start limit hit for %s (StartLimitBurst=%d)\n",
            unit_name_or_unknown(entry->unit), burst);
    return 0;
}

static int spawn_service(ServiceEntry *entry) {
    if (!entry || !entry->unit) {
        fprintf(stderr, "[service_manager] Cannot spawn service from a NULL entry or unit\n");
        return -1;
    }

    if (!unit_field_is_nonempty(entry->unit->exec_start, sizeof(entry->unit->exec_start), "ExecStart", entry->unit)) {
        entry->state = SERVICE_FAILED;
        entry->pid = -1;
        return -1;
    }

    if (!start_limit_allows(entry)) {
        entry->state = SERVICE_FAILED;
        entry->pid = -1;
        return -1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        if (bounded_strlen(entry->unit->exec_start, sizeof(entry->unit->exec_start), NULL) != 0) {
            fprintf(stderr, "[service_manager] Refusing to execute unterminated ExecStart for %s\n",
                    unit_name_or_unknown(entry->unit));
            _exit(127);
        }
        execl("/bin/sh", "sh", "-c", entry->unit->exec_start, NULL);
        perror("exec failed");
        _exit(127);
    }

    if (pid < 0) {
        perror("fork failed");
        entry->state = SERVICE_FAILED;
        return -1;
    }

    entry->pid = pid;
    entry->state = SERVICE_ACTIVE;
    entry->stopping = 0;
    entry->restart_count++;

    fprintf(stderr, "[service_manager] Started %s (PID %d, start #%d)\n",
            unit_name_or_unknown(entry->unit), pid, entry->restart_count);
    return 0;
}

static ServiceEntry *find_entry(Unit *unit) {
    if (!unit)
        return NULL;

    for (size_t i = 0; i < service_count; i++) {
        if (service_table[i].unit == unit)
            return &service_table[i];
    }
    return NULL;
}

static int on_restart_timer(sd_event_source *s, uint64_t usec, void *userdata) {
    (void)usec;
    RestartJob *job = userdata;
    ServiceEntry *entry = job ? job->entry : NULL;

    sd_event_source_unref(s);
    free(job);

    if (!entry || !entry->unit || entry->stopping)
        return 0;

    fprintf(stderr, "[service_manager] Restarting %s\n", unit_name_or_unknown(entry->unit));
    spawn_service(entry);
    return 0;
}

static void schedule_restart(ServiceEntry *entry) {
    if (!entry || !entry->unit) {
        fprintf(stderr, "[service_manager] Cannot schedule restart for a NULL service entry\n");
        return;
    }

    int delay = parse_seconds_or_default(entry->unit->restart_sec, sizeof(entry->unit->restart_sec), 1);

    if (!event) {
        fprintf(stderr, "[service_manager] No event loop available; cannot restart %s\n", unit_name_or_unknown(entry->unit));
        entry->state = SERVICE_FAILED;
        return;
    }

    RestartJob *job = calloc(1, sizeof(*job));
    if (!job) {
        fprintf(stderr, "[service_manager] Failed to allocate restart job for %s\n", unit_name_or_unknown(entry->unit));
        entry->state = SERVICE_FAILED;
        return;
    }
    job->entry = entry;

    uint64_t now;
    int r = sd_event_now(event, CLOCK_MONOTONIC, &now);
    if (r < 0) {
        fprintf(stderr, "[service_manager] Failed to read event clock for restart: %s\n", strerror(-r));
        free(job);
        entry->state = SERVICE_FAILED;
        return;
    }

    sd_event_source *source = NULL;
    r = sd_event_add_time(event, &source, CLOCK_MONOTONIC,
                          now + (uint64_t)delay * 1000000, 0,
                          on_restart_timer, job);
    if (r < 0) {
        fprintf(stderr, "[service_manager] Failed to schedule restart for %s: %s\n",
                unit_name_or_unknown(entry->unit), strerror(-r));
        free(job);
        entry->state = SERVICE_FAILED;
        return;
    }

    entry->state = SERVICE_STARTING;
    fprintf(stderr, "[service_manager] Scheduled restart for %s in %ds\n",
            unit_name_or_unknown(entry->unit), delay);
}

int service_manager_start(Unit *unit) {
    if (!unit) {
        fprintf(stderr, "[service_manager] Cannot start a NULL unit\n");
        return -1;
    }

    if (unit->type != UNIT_SERVICE ||
        !unit_field_is_nonempty(unit->exec_start, sizeof(unit->exec_start), "ExecStart", unit)) {
        fprintf(stderr, "[service_manager] Not a valid service unit\n");
        return -1;
    }

    ServiceEntry *entry = find_entry(unit);
    if (entry) {
        if (entry->pid > 0 && kill(entry->pid, 0) == 0) {
            fprintf(stderr, "[service_manager] Service %s already running (PID %d), not starting duplicate\n",
                    unit_name_or_unknown(unit), entry->pid);
            return 0;
        }

        if (entry->pid > 0) {
            fprintf(stderr, "[service_manager] Service %s stale PID %d is gone; marking inactive\n",
                    unit_name_or_unknown(unit), entry->pid);
            entry->pid = -1;
        }
        entry->state = SERVICE_INACTIVE;
        entry->stopping = 0;
        return spawn_service(entry);
    }

    const CoreinitdConfig *config = coreinitd_config_get();
    size_t max_services = config ? config->max_services : COREINITD_MAX_SERVICES_CAPACITY;
    if (max_services > COREINITD_MAX_SERVICES_CAPACITY) {
        fprintf(stderr, "[service_manager] Configured max_services=%zu exceeds compiled capacity %d; clamping\n",
                max_services, COREINITD_MAX_SERVICES_CAPACITY);
        max_services = COREINITD_MAX_SERVICES_CAPACITY;
    }

    if (service_count >= max_services) {
        fprintf(stderr, "[service_manager] Service table full (%zu)\n", max_services);
        return -1;
    }

    ServiceEntry *new_entry = &service_table[service_count++];
    *new_entry = (ServiceEntry){
        .unit = unit,
        .pid = -1,
        .state = SERVICE_INACTIVE,
        .last_status = 0,
        .restart_count = 0,
        .stopping = 0
    };

    return spawn_service(new_entry);
}

void service_manager_reap_status(pid_t pid, int status) {
    for (size_t i = 0; i < service_count; i++) {
        ServiceEntry *entry = &service_table[i];
        if (entry->pid != pid)
            continue;

        entry->last_status = status;
        entry->pid = -1;

        if (entry->stopping) {
            entry->state = SERVICE_INACTIVE;
            fprintf(stderr, "[service_manager] Stopped %s (PID %d)\n", unit_name_or_unknown(entry->unit), pid);
            return;
        }

        int success = status_is_success(entry->unit, status);
        entry->state = success ? SERVICE_INACTIVE : SERVICE_FAILED;

        if (WIFEXITED(status)) {
            fprintf(stderr, "[service_manager] Reaped %s (PID %d, exit %d, %s)\n",
                    unit_name_or_unknown(entry->unit), pid, WEXITSTATUS(status), success ? "success" : "failure");
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "[service_manager] Reaped %s (PID %d, signal %d, %s)\n",
                    unit_name_or_unknown(entry->unit), pid, WTERMSIG(status), success ? "success" : "failure");
        } else {
            fprintf(stderr, "[service_manager] Reaped %s (PID %d)\n", unit_name_or_unknown(entry->unit), pid);
        }

        if (restart_policy_allows(entry->unit, status))
            schedule_restart(entry);

        return;
    }
}

void service_manager_reap(pid_t pid) {
    service_manager_reap_status(pid, 0);
}

void service_manager_status(void) {
    for (size_t i = 0; i < service_count; i++) {
        printf("%s\tPID %d\t%s\trestarts %d\n",
               unit_name_or_unknown(service_table[i].unit),
               service_table[i].pid,
               service_state_name(service_table[i].state),
               service_table[i].restart_count > 0 ? service_table[i].restart_count - 1 : 0);
    }
}

void service_manager_stop_all(void) {
    if (service_count == 0) {
        fprintf(stderr, "[service_manager] No services to stop\n");
        return;
    }

    fprintf(stderr, "[service_manager] Stopping %zu service(s)...\n", service_count);

    for (size_t i = 0; i < service_count; i++) {
        ServiceEntry *entry = &service_table[i];
        entry->stopping = 1;
        if (entry->pid > 0) {
            entry->state = SERVICE_STOPPING;
            fprintf(stderr, "[service_manager] Sending SIGTERM to %s (PID %d)\n",
                    unit_name_or_unknown(entry->unit), entry->pid);
            kill(entry->pid, SIGTERM);
        }
    }

    sleep(1);

    for (size_t i = 0; i < service_count; i++) {
        ServiceEntry *entry = &service_table[i];
        if (entry->pid > 0 && kill(entry->pid, 0) == 0) {
            fprintf(stderr, "[service_manager] Sending SIGKILL to %s (PID %d)\n",
                    unit_name_or_unknown(entry->unit), entry->pid);
            kill(entry->pid, SIGKILL);
        }
    }

    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        service_manager_reap_status(pid, status);

    fprintf(stderr, "[service_manager] All services stopped\n");
}
