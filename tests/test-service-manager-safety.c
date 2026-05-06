#include "../src/coreinitd/service_manager.h"
#include <stdio.h>
#include <string.h>
#include <systemd/sd-event.h>

sd_event *event = NULL;

static int expect_start_rejected(const char *label, Unit *unit) {
    if (service_manager_start(unit) == 0) {
        fprintf(stderr, "%s: expected service_manager_start to reject unsafe input\n", label);
        service_manager_stop_all();
        return 1;
    }
    return 0;
}

int main() {
    if (expect_start_rejected("NULL unit", NULL) != 0)
        return 1;

    Unit empty_exec;
    memset(&empty_exec, 0, sizeof(empty_exec));
    empty_exec.type = UNIT_SERVICE;
    snprintf(empty_exec.name, sizeof(empty_exec.name), "%s", "empty-exec.service");
    if (expect_start_rejected("empty ExecStart", &empty_exec) != 0)
        return 1;

    Unit unterminated_exec;
    memset(&unterminated_exec, 0, sizeof(unterminated_exec));
    unterminated_exec.type = UNIT_SERVICE;
    snprintf(unterminated_exec.name, sizeof(unterminated_exec.name), "%s", "unterminated-exec.service");
    memset(unterminated_exec.exec_start, 'x', sizeof(unterminated_exec.exec_start));
    if (expect_start_rejected("unterminated ExecStart", &unterminated_exec) != 0)
        return 1;

    Unit wrong_type;
    memset(&wrong_type, 0, sizeof(wrong_type));
    wrong_type.type = UNIT_SOCKET;
    snprintf(wrong_type.name, sizeof(wrong_type.name), "%s", "wrong-type.socket");
    snprintf(wrong_type.exec_start, sizeof(wrong_type.exec_start), "%s", "/bin/true");
    if (expect_start_rejected("wrong unit type", &wrong_type) != 0)
        return 1;

    printf("service manager rejected NULL, empty, unterminated, and wrong-type units\n");
    return 0;
}
