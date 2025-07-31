// main.c — coreinitd event loop skeleton
#include <systemd/sd-event.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

static int on_sigchld(sd_event_source *s, const struct signalfd_siginfo *si, void *userdata) {
    pid_t pid;
    int status;

    // Reap all exited children
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        fprintf(stderr, "[coreinitd] Reaped child PID %d\n", pid);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    sd_event *event = NULL;
    int r;

    // Create event loop
    r = sd_event_default(&event);
    if (r < 0) {
        fprintf(stderr, "Failed to create event loop: %s\n", strerror(-r));
        return 1;
    }

    // Handle SIGCHLD to track service deaths
    sd_event_source *sigchld_src = NULL;
    r = sd_event_add_signal(event, &sigchld_src, SIGCHLD, on_sigchld, NULL);
    if (r < 0) {
        fprintf(stderr, "Failed to add SIGCHLD handler: %s\n", strerror(-r));
        return 1;
    }

    sd_event_source_set_priority(sigchld_src, SD_EVENT_PRIORITY_NORMAL);

    fprintf(stderr, "[coreinitd] Starting event loop...\n");

    // Main loop — run forever
    r = sd_event_loop(event);
    if (r < 0) {
        fprintf(stderr, "Event loop failed: %s\n", strerror(-r));
        return 1;
    }

    sd_event_unref(event);
    return 0;
}
