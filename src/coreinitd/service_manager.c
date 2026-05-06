#include "service_manager.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "config.h"

static ServiceEntry service_table[COREINITD_MAX_SERVICES_CAPACITY];
static size_t service_count = 0;

int service_manager_start(Unit *unit) {
    if (unit->type != UNIT_SERVICE || strlen(unit->exec_start) == 0) {
        fprintf(stderr, "[service_manager] Not a valid service unit\n");
        return -1;
    }

    for (size_t i = 0; i < service_count; i++) {
        if (service_table[i].unit == unit && service_table[i].pid > 0) {
            if (kill(service_table[i].pid, 0) == 0) {
                fprintf(stderr, "[service_manager] Service %s already running (PID %d), not starting duplicate\n",
                        unit->name, service_table[i].pid);
                return 0;
            }

            fprintf(stderr, "[service_manager] Service %s stale PID %d is gone; marking inactive\n",
                    unit->name, service_table[i].pid);
            service_table[i].state = SERVICE_INACTIVE;
            service_table[i].pid = -1;
        }
    }

    const CoreinitdConfig *config = coreinitd_config_get();
    if (service_count >= config->max_services) {
        fprintf(stderr, "[service_manager] Service table full (%zu)\n", config->max_services);
        return -1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // child
        execl("/bin/sh", "sh", "-c", unit->exec_start, NULL);
        perror("exec failed");
        _exit(1);
    }

    if (pid < 0) {
        perror("fork failed");
        return -1;
    }

    service_table[service_count++] = (ServiceEntry){
        .unit = unit,
        .pid = pid,
        .state = SERVICE_STARTING
    };

    fprintf(stderr, "[service_manager] Started %s (PID %d)\n", unit->name, pid);
    return 0;
}

void service_manager_reap(pid_t pid) {
    for (size_t i = 0; i < service_count; i++) {
        if (service_table[i].pid == pid) {
            fprintf(stderr, "[service_manager] Reaped %s (PID %d)\n", service_table[i].unit->name, pid);
            service_table[i].state = SERVICE_ACTIVE;  // or SERVICE_FAILED if needed
            return;
        }
    }
}

void service_manager_status(void) {
    for (size_t i = 0; i < service_count; i++) {
        const char *state = "unknown";
        switch (service_table[i].state) {
            case SERVICE_INACTIVE: state = "inactive"; break;
            case SERVICE_STARTING: state = "starting"; break;
            case SERVICE_ACTIVE: state = "active"; break;
            case SERVICE_FAILED: state = "failed"; break;
        }
        printf("%s\tPID %d\t%s\n", service_table[i].unit->name, service_table[i].pid, state);
    }
}

/**
 * service_manager_stop_all() - Gracefully stop all running services
 *
 * Three-phase shutdown:
 * 1. Send SIGTERM to all services (graceful shutdown request)
 * 2. Wait 1 second for services to clean up
 * 3. Send SIGKILL to any remaining services (force terminate)
 * 4. Reap all child processes
 *
 * Returns 0 on success, negative on error.
 */
void service_manager_stop_all(void) {
    if (service_count == 0) {
        fprintf(stderr, "[service_manager] No services to stop\n");
        return;
    }

    fprintf(stderr, "[service_manager] Stopping %zu service(s)...\n", service_count);

    // Phase 1: Send SIGTERM to all services (graceful shutdown)
    for (size_t i = 0; i < service_count; i++) {
        if (service_table[i].pid > 0) {
            fprintf(stderr, "[service_manager] Sending SIGTERM to %s (PID %d)\n",
                    service_table[i].unit->name, service_table[i].pid);
            kill(service_table[i].pid, SIGTERM);
        }
    }

    // Phase 2: Wait 1 second for graceful shutdown
    sleep(1);

    // Phase 3: Send SIGKILL to any stragglers
    for (size_t i = 0; i < service_count; i++) {
        if (service_table[i].pid > 0) {
            // Check if process still exists by sending signal 0 (no-op)
            if (kill(service_table[i].pid, 0) == 0) {
                fprintf(stderr, "[service_manager] Sending SIGKILL to %s (PID %d)\n",
                        service_table[i].unit->name, service_table[i].pid);
                kill(service_table[i].pid, SIGKILL);
            }
        }
    }

    // Phase 4: Reap all children
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        fprintf(stderr, "[service_manager] Reaped child PID %d\n", pid);
    }

    fprintf(stderr, "[service_manager] All services stopped\n");
}
