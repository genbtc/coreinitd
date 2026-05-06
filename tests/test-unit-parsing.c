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

static int expect_int(const char *label, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s: got %d, want %d\n", label, got, want);
        return 1;
    }
    return 0;
}

static int write_verbs_fixture(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("fopen verbs fixture");
        return 1;
    }

    fputs("[Unit]\n"
          "Description=Verb Coverage Service\n"
          "After=graphical-session.target\n"
          "Documentation=man:coreinitd(8)\n"
          "PartOf=graphical-session.target\n"
          "Requires=verb.socket\n"
          "StartLimitBurst=4\n"
          "StartLimitIntervalSec=60\n"
          "[Service]\n"
          "AmbientCapabilities=CAP_CHOWN CAP_FOWNER\n"
          "BusName=org.example.Verb\n"
          "Environment=FOO=bar\n"
          "ExecReload=/bin/true reload\n"
          "ExecStart=/bin/true\n"
          "ExecStartPost=-/bin/true post\n"
          "KillMode=process\n"
          "MemoryDenyWriteExecute=true\n"
          "NoNewPrivileges=true\n"
          "Restart=on-failure\n"
          "RestartForceExitStatus=3 4\n"
          "RestartSec=1\n"
          "Slice=session.slice\n"
          "SuccessExitStatus=15\n"
          "SystemCallArchitectures=native\n"
          "TimeoutStopSec=5\n"
          "Type=dbus\n"
          "[Install]\n"
          "WantedBy=default.target\n",
          f);

    fclose(f);
    return 0;
}

static int write_socket_verbs_fixture(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("fopen socket verbs fixture");
        return 1;
    }

    fputs("[Socket]\n"
          "DirectoryMode=0700\n"
          "FileDescriptorName=std\n"
          "ListenStream=%t/verb.sock\n"
          "Service=verb.service\n"
          "SocketMode=0600\n",
          f);

    fclose(f);
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

    if (write_verbs_fixture("build/verb-coverage.service") != 0 ||
        write_socket_verbs_fixture("build/verb-coverage.socket") != 0) {
        return 1;
    }

    Unit verbs;
    if (load_unit("build/verb-coverage.service", &verbs) != 0) {
        fprintf(stderr, "Failed to load verb coverage service\n");
        return 1;
    }

    if (expect_str("Description", verbs.description, "Verb Coverage Service") ||
        expect_str("After", verbs.after.values[0], "graphical-session.target") ||
        expect_str("Documentation", verbs.documentation.values[0], "man:coreinitd(8)") ||
        expect_str("PartOf", verbs.part_of.values[0], "graphical-session.target") ||
        expect_str("Requires", verbs.requires.values[0], "verb.socket") ||
        expect_int("StartLimitBurst", verbs.start_limit_burst, 4) ||
        expect_str("StartLimitIntervalSec", verbs.start_limit_interval_sec, "60") ||
        expect_str("AmbientCapabilities", verbs.ambient_capabilities.values[0], "CAP_CHOWN CAP_FOWNER") ||
        expect_str("BusName", verbs.bus_name, "org.example.Verb") ||
        expect_str("Environment", verbs.environment.values[0], "FOO=bar") ||
        expect_str("ExecReload", verbs.exec_reload, "/bin/true reload") ||
        expect_str("ExecStart", verbs.exec_start, "/bin/true") ||
        expect_str("ExecStartPost", verbs.exec_start_post.values[0], "-/bin/true post") ||
        expect_str("KillMode", verbs.kill_mode, "process") ||
        expect_int("MemoryDenyWriteExecute", verbs.memory_deny_write_execute, 1) ||
        expect_int("NoNewPrivileges", verbs.no_new_privileges, 1) ||
        expect_str("Restart", verbs.restart, "on-failure") ||
        expect_str("RestartForceExitStatus", verbs.restart_force_exit_status.values[0], "3 4") ||
        expect_str("RestartSec", verbs.restart_sec, "1") ||
        expect_str("Slice", verbs.slice, "session.slice") ||
        expect_str("SuccessExitStatus", verbs.success_exit_status.values[0], "15") ||
        expect_str("SystemCallArchitectures", verbs.system_call_architectures, "native") ||
        expect_str("TimeoutStopSec", verbs.timeout_stop_sec, "5") ||
        expect_str("Type", verbs.type_name, "dbus") ||
        expect_str("WantedBy", verbs.wanted_by.values[0], "default.target")) {
        return 1;
    }

    Unit socket_verbs;
    if (load_unit("build/verb-coverage.socket", &socket_verbs) != 0) {
        fprintf(stderr, "Failed to load verb coverage socket\n");
        return 1;
    }

    if (expect_str("DirectoryMode", socket_verbs.directory_mode, "0700") ||
        expect_str("FileDescriptorName", socket_verbs.file_descriptor_name, "std") ||
        expect_str("ListenStream", socket_verbs.listen_stream, "%t/verb.sock") ||
        expect_str("Service", socket_verbs.service, "verb.service") ||
        expect_str("SocketMode", socket_verbs.socket_mode, "0600")) {
        return 1;
    }

    printf("Loaded example service, UNIX socket, timer, and verb coverage units successfully\n");
    return 0;
}
