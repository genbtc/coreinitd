// main.c — coreinitd unified daemon with event loop + timerd spawning
#include <systemd/sd-event.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <dirent.h>

#define UNIT_DIR "./etc/units"

#include "unit_loader.h"
#define MAX_UNITS 64
static Unit loaded_units[MAX_UNITS];
static size_t unit_count = 0;

static sd_event *event = NULL;

// ─────────────────────────────────────────────────────────────
// Event loop wrapper (inline version of event_loop.c)
// ─────────────────────────────────────────────────────────────

static int on_sigchld(sd_event_source *s, const struct signalfd_siginfo *si, void *userdata) {
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        fprintf(stderr, "[coreinitd] Reaped child PID %d\n", pid);
    }

    return 0;
}

int event_loop_init(void) {
    int r = sd_event_default(&event);
    if (r < 0) {
        fprintf(stderr, "[coreinitd] Failed to create event loop: %s\n", strerror(-r));
        return -1;
    }

    sd_event_source *sigchld_src = NULL;
    r = sd_event_add_signal(event, &sigchld_src, SIGCHLD, on_sigchld, NULL);
    if (r < 0) {
        fprintf(stderr, "[coreinitd] Failed to add SIGCHLD handler: %s\n", strerror(-r));
        return -1;
    }

    sd_event_source_set_priority(sigchld_src, SD_EVENT_PRIORITY_NORMAL);
    return 0;
}

int event_loop_run(void) {
    fprintf(stderr, "[coreinitd] Starting event loop...\n");
    int r = sd_event_loop(event);
    if (r < 0) {
        fprintf(stderr, "[coreinitd] Event loop error: %s\n", strerror(-r));
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

// ─────────────────────────────────────────────────────────────
// Timer unit launcher (replaces spawn_timerd_for + loop)
// ─────────────────────────────────────────────────────────────

void spawn_timerd_for(const char *filename) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", UNIT_DIR, filename);

    pid_t pid = fork();
    if (pid == 0) {
        // child
        execl("./build/helpers/timerd", "timerd", path, NULL);
        perror("execl failed for timerd");
        _exit(1);
    } else if (pid < 0) {
        perror("fork failed for timerd");
    } else {
        fprintf(stderr, "[coreinitd] Started timerd for %s (PID %d)\n", filename, pid);
    }
}

void load_all_units(void) {
    DIR *d = opendir(UNIT_DIR);
    if (!d) {
        perror("opendir");
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (!(strstr(ent->d_name, ".service") ||
            strstr(ent->d_name, ".socket") ||
            strstr(ent->d_name, ".timer"))) continue;

        if (unit_count >= MAX_UNITS) {
            fprintf(stderr, "[coreinitd] Unit limit reached\n");
            break;
        }

        char path[256];
        snprintf(path, sizeof(path), "%s/%s", UNIT_DIR, ent->d_name);
        const char *type_str = "unknown";

        if (load_unit(path, &loaded_units[unit_count]) == 0) {
            switch (loaded_units[unit_count].type) {
                case UNIT_SERVICE: type_str = "service"; break;
                case UNIT_SOCKET: type_str = "socket"; break;
                case UNIT_TIMER: type_str = "timer"; break;
                default: break;
            }
            fprintf(stderr, "[coreinitd] Loaded %s unit: %s → %s\n",
                type_str, ent->d_name, loaded_units[unit_count].exec_start);
            unit_count++;
        } else {
            fprintf(stderr, "[coreinitd] Failed to load %s\n", ent->d_name);
        }
    }

    closedir(d);
}

void spawn_all_timer_units(void) {
    DIR *d = opendir(UNIT_DIR);
    if (!d) {
        perror("opendir");
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (strstr(ent->d_name, ".timer")) {
            spawn_timerd_for(ent->d_name);
        }
    }

    closedir(d);
}

// ─────────────────────────────────────────────────────────────
// Main Entry Point
// ─────────────────────────────────────────────────────────────

int main(void) {
    if (event_loop_init() < 0)
        return 1;

    load_all_units();           // Parses and loads .service files
    spawn_all_timer_units();    // Spawns timerd for .timer files

    int ret = event_loop_run();
    event_loop_shutdown();
    return ret;
}
