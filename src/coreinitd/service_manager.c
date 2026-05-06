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

static int parse_seconds_or_default(const char *value, int fallback) {
    int seconds;
    if (value[0] == '\0')
        return fallback;
    if (parse_sec_to_int(value, &seconds) == 0)
        return seconds;
    fprintf(stderr, "[service_manager] Invalid time value '%s', using %ds\n", value, fallback);
    return fallback;
}

static int list_contains_status(const UnitStringList *list, int code) {
    char needle[32];
    snprintf(needle, sizeof(needle), "%d", code);

    for (size_t i = 0; i < list->count; i++) {
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
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        return code == 0 || list_contains_status(&unit->success_exit_status, code);
    }

    if (WIFSIGNALED(status))
        return list_contains_status(&unit->success_exit_status, WTERMSIG(status));

    return 0;
}

static int status_forces_restart(const Unit *unit, int status) {
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
    const char *policy = unit->restart[0] ? unit->restart : "no";

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
            policy, unit->name);
    return 0;
}

static int start_limit_allows(ServiceEntry *entry) {
    int burst = entry->unit->start_limit_burst;
    if (burst <= 0)
        return 1;

    if (entry->restart_count < burst)
        return 1;

    fprintf(stderr, "[service_manager] Start limit hit for %s (StartLimitBurst=%d)\n",
            entry->unit->name, burst);
    return 0;
}

static int spawn_service(ServiceEntry *entry) {
    if (!start_limit_allows(entry)) {
        entry->state = SERVICE_FAILED;
        entry->pid = -1;
        return -1;
    }

    pid_t pid = fork();
    if (pid == 0) {
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
            entry->unit->name, pid, entry->restart_count);
    return 0;
}

static ServiceEntry *find_entry(Unit *unit) {
    for (size_t i = 0; i < service_count; i++) {
        if (service_table[i].unit == unit)
            return &service_table[i];
    }
    return NULL;
}

static int on_restart_timer(sd_event_source *s, uint64_t usec, void *userdata) {
    (void)usec;
    RestartJob *job = userdata;
    ServiceEntry *entry = job->entry;

    sd_event_source_unref(s);
    free(job);

    if (!entry || entry->stopping)
        return 0;

    fprintf(stderr, "[service_manager] Restarting %s\n", entry->unit->name);
    spawn_service(entry);
    return 0;
}

static void schedule_restart(ServiceEntry *entry) {
    int delay = parse_seconds_or_default(entry->unit->restart_sec, 1);

    if (!event) {
        fprintf(stderr, "[service_manager] No event loop available; cannot restart %s\n", entry->unit->name);
        entry->state = SERVICE_FAILED;
        return;
    }

    RestartJob *job = calloc(1, sizeof(*job));
    if (!job) {
        fprintf(stderr, "[service_manager] Failed to allocate restart job for %s\n", entry->unit->name);
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
                entry->unit->name, strerror(-r));
        free(job);
        entry->state = SERVICE_FAILED;
        return;
    }

    entry->state = SERVICE_STARTING;
    fprintf(stderr, "[service_manager] Scheduled restart for %s in %ds\n",
            entry->unit->name, delay);
}

int service_manager_start(Unit *unit) {
    if (unit->type != UNIT_SERVICE || strlen(unit->exec_start) == 0) {
        fprintf(stderr, "[service_manager] Not a valid service unit\n");
        return -1;
    }

    ServiceEntry *entry = find_entry(unit);
    if (entry) {
        if (entry->pid > 0 && kill(entry->pid, 0) == 0) {
            fprintf(stderr, "[service_manager] Service %s already running (PID %d), not starting duplicate\n",
                    unit->name, entry->pid);
            return 0;
        }

        if (entry->pid > 0) {
            fprintf(stderr, "[service_manager] Service %s stale PID %d is gone; marking inactive\n",
                    unit->name, entry->pid);
            entry->pid = -1;
        }
        entry->state = SERVICE_INACTIVE;
        entry->stopping = 0;
        return spawn_service(entry);
    }

    const CoreinitdConfig *config = coreinitd_config_get();
    if (service_count >= config->max_services) {
        fprintf(stderr, "[service_manager] Service table full (%zu)\n", config->max_services);
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
            fprintf(stderr, "[service_manager] Stopped %s (PID %d)\n", entry->unit->name, pid);
            return;
        }

        int success = status_is_success(entry->unit, status);
        entry->state = success ? SERVICE_INACTIVE : SERVICE_FAILED;

        if (WIFEXITED(status)) {
            fprintf(stderr, "[service_manager] Reaped %s (PID %d, exit %d, %s)\n",
                    entry->unit->name, pid, WEXITSTATUS(status), success ? "success" : "failure");
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "[service_manager] Reaped %s (PID %d, signal %d, %s)\n",
                    entry->unit->name, pid, WTERMSIG(status), success ? "success" : "failure");
        } else {
            fprintf(stderr, "[service_manager] Reaped %s (PID %d)\n", entry->unit->name, pid);
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
               service_table[i].unit->name,
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
                    entry->unit->name, entry->pid);
            kill(entry->pid, SIGTERM);
        }
    }

    sleep(1);

    for (size_t i = 0; i < service_count; i++) {
        ServiceEntry *entry = &service_table[i];
        if (entry->pid > 0 && kill(entry->pid, 0) == 0) {
            fprintf(stderr, "[service_manager] Sending SIGKILL to %s (PID %d)\n",
                    entry->unit->name, entry->pid);
            kill(entry->pid, SIGKILL);
        }
    }

    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        service_manager_reap_status(pid, status);

    fprintf(stderr, "[service_manager] All services stopped\n");
}
