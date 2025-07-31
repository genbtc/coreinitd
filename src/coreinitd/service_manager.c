#include "service_manager.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_SERVICES 64
static ServiceEntry service_table[MAX_SERVICES];
static size_t service_count = 0;

int service_manager_start(Unit *unit) {
    if (unit->type != UNIT_SERVICE || strlen(unit->exec_start) == 0) {
        fprintf(stderr, "[service_manager] Not a valid service unit\n");
        return -1;
    }

    if (service_count >= MAX_SERVICES) {
        fprintf(stderr, "[service_manager] Service table full\n");
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
