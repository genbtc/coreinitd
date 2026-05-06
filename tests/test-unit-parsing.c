#include "../src/coreinitd/unit_loader.h"
#include <stdio.h>
#include <string.h>

static int expect_str(const char *label, const char *got, const char *want) {
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "%s: got '%s', want '%s'\n", label, got, want);
        return 1;
    }
    return 0;
}

int main() {
    Unit service;
    if (load_unit("etc/units/example.service", &service) != 0) {
        fprintf(stderr, "Failed to load example.service\n");
        return 1;
    }

    if (expect_str("ExecStart", service.exec_start, "build/example-daemon") ||
        expect_str("NotifyAccess", service.notify_access, "main") ||
        expect_str("Socket", service.socket_unit, "example.socket")) {
        return 1;
    }

    Unit socket;
    if (load_unit("etc/units/example.socket", &socket) != 0) {
        fprintf(stderr, "Failed to load example.socket\n");
        return 1;
    }

    if (socket.type != UNIT_SOCKET ||
        expect_str("ListenStream", socket.listen_stream, "/tmp/coreinitd-example.sock")) {
        return 1;
    }

    Unit timer;
    if (load_unit("etc/units/example.timer", &timer) != 0) {
        fprintf(stderr, "Failed to load example.timer\n");
        return 1;
    }

    if (timer.type != UNIT_TIMER ||
        expect_str("OnBootSec", timer.on_boot_sec, "10s") ||
        expect_str("OnUnitActiveSec", timer.on_active_sec, "1h") ||
        expect_str("Unit", timer.timer_unit, "example.service")) {
        return 1;
    }

    printf("Loaded example service, UNIX socket, and timer units successfully\n");
    return 0;
}
