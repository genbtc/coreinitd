// event_loop.c — sd_event loop wrapper for coreinitd
#include "event_loop.h"
#include <systemd/sd-event.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

// Basic SIGCHLD handler: reaps children
static int on_sigchld(sd_event_source *s, const struct signalfd_siginfo *si, void *userdata) {
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        fprintf(stderr, "[coreinitd-event] Reaped child PID %d\n", pid);
    }

    return 0;
}

int event_loop_init(void) {
    // Register SIGCHLD
    static sd_event_source *sigchld_src = NULL;
    int r = sd_event_default(&event);
    if (r < 0) {
        fprintf(stderr, "[coreinitd-event] Failed to create event loop: %s\n", strerror(-r));
        return -1;
    }
    if (sigchld_src == NULL) {
        r = sd_event_add_signal(event, &sigchld_src, SIGCHLD, on_sigchld, NULL);
        if (r < 0) {
            fprintf(stderr, "[coreinitd-event] Failed to add SIGCHLD handler: %s\n", strerror(-r));
            return -1;
        }
        sd_event_source_set_priority(sigchld_src, SD_EVENT_PRIORITY_NORMAL);
    } else {
        fprintf(stderr, "[coreinitd-event] SIGCHLD handler already registered.\n");
    }

    return 0;
}

int event_loop_run(void) {
    fprintf(stderr, "[coreinitd-event] Starting event loop...\n");
    int r = sd_event_loop(event);
    if (r < 0) {
        fprintf(stderr, "[coreinitd-event] Event loop error: %s\n", strerror(-r));
        return -1;
    }
    return 0;
}

void event_loop_shutdown(void) {
    if (event) {
        sd_event_unref(event);
        event = NULL;
    }
}
