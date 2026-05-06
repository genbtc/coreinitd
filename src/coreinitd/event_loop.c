// event_loop.c — sd_event loop wrapper for coreinitd
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <systemd/sd-event.h>
#include "event_loop.h"
#include "service_manager.h"

// Global event loop pointer - accessible to all modules
sd_event *event = NULL;

static sd_event_source *sigchld_src = NULL;
static sd_event_source *sigint_src = NULL;
static sd_event_source *sigterm_src = NULL;

// Basic SIGCHLD handler: reaps children
static int on_sigchld(sd_event_source *s, const struct signalfd_siginfo *si, void *userdata) {
    (void)s;
    (void)si;
    (void)userdata;

    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        service_manager_reap_status(pid, status);
    }

    return 0;
}

static int on_termination_signal(sd_event_source *s, const struct signalfd_siginfo *si, void *userdata) {
    (void)userdata;

    sd_event *loop = sd_event_source_get_event(s);
    fprintf(stderr, "[coreinitd-event] Received signal %d, leaving event loop\n", si->ssi_signo);
    return sd_event_exit(loop, 0);
}

static int block_signal(int signo) {
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, signo);

    if (sigprocmask(SIG_BLOCK, &ss, NULL) < 0) {
        fprintf(stderr, "[coreinitd-event] Failed to block signal %d: %s\n", signo, strerror(errno));
        return -errno;
    }

    return 0;
}

// Register signal handlers used by the main event loop.
int event_loop_init(void) {
    int r;

    if (!event) {
        r = sd_event_default(&event);
        if (r < 0) {
            fprintf(stderr, "[coreinitd-event] Failed to create event loop: %s\n", strerror(-r));
            return r;
        }
        fprintf(stderr, "[coreinitd-event] Created default sd-event loop\n");
    }

    int signals[] = { SIGCHLD, SIGINT, SIGTERM };
    for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
        r = block_signal(signals[i]);
        if (r < 0) return r;
    }

    if (sigchld_src == NULL) {
        r = sd_event_add_signal(event, &sigchld_src, SIGCHLD, on_sigchld, NULL);
        if (r < 0) {
            fprintf(stderr, "[coreinitd-event] Failed to add SIGCHLD handler: %s (%d)\n", strerror(-r), -r);
            return r;
        }
        sd_event_source_set_priority(sigchld_src, SD_EVENT_PRIORITY_NORMAL);
        fprintf(stderr, "[coreinitd-event] SIGCHLD handler registered\n");
    }

    if (sigint_src == NULL) {
        r = sd_event_add_signal(event, &sigint_src, SIGINT, on_termination_signal, NULL);
        if (r < 0) {
            fprintf(stderr, "[coreinitd-event] Failed to add SIGINT handler: %s (%d)\n", strerror(-r), -r);
            return r;
        }
        fprintf(stderr, "[coreinitd-event] SIGINT handler registered\n");
    }

    if (sigterm_src == NULL) {
        r = sd_event_add_signal(event, &sigterm_src, SIGTERM, on_termination_signal, NULL);
        if (r < 0) {
            fprintf(stderr, "[coreinitd-event] Failed to add SIGTERM handler: %s (%d)\n", strerror(-r), -r);
            return r;
        }
        fprintf(stderr, "[coreinitd-event] SIGTERM handler registered\n");
    }

    return 0;
}

int event_loop_run(void) {
    if (!event) {
        fprintf(stderr, "[coreinitd-event] Cannot start event loop before event_loop_init()\n");
        return -1;
    }

    fprintf(stderr, "[coreinitd-event] Starting event loop...\n");
    int r = sd_event_loop(event);
    if (r < 0) {
        fprintf(stderr, "[coreinitd-event] Event loop error: %s\n", strerror(-r));
        return -1;
    }
    fprintf(stderr, "[coreinitd-event] Event loop stopped with status %d\n", r);
    return r;
}

void event_loop_shutdown(void) {
    if (sigchld_src) sigchld_src = sd_event_source_unref(sigchld_src);
    if (sigint_src) sigint_src = sd_event_source_unref(sigint_src);
    if (sigterm_src) sigterm_src = sd_event_source_unref(sigterm_src);

    if (event) {
        event = sd_event_unref(event);
        fprintf(stderr, "[coreinitd-event] Event loop released\n");
    }
}
