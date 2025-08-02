// main.c — coreinitd unified daemon with event loop + timerd spawning
// 					  systemd executor + socket activation
// 2025(C) genr8eofl - @ gentoo libera IRC
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

#include "service_manager.h"
#include "socket_activation.h"
#include "event_loop.h"

// ───────────────────
// Timer unit launcher
// ───────────────────
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
        perror("opendir failed");
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

        char path[512];
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
        perror("opendir failed");
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (strstr(ent->d_name, ".timer"))
            spawn_timerd_for(ent->d_name);
    }

    closedir(d);
}

// ─────────────────
// Main Entry Point
// ─────────────────
int main(void) {
    fprintf(stderr, "[coreinitd-main] Starting...\n");
//TODO: [coreinitd-event] SIGCHLD already has a handler!
//    if (event_loop_init() < 0)
//        return 1;

    load_all_units();           // Parses and loads .service files
    //Starts all valid services
    for (size_t i = 0; i < unit_count; i++) {
        if (loaded_units[i].type == UNIT_SERVICE) {
            service_manager_start(&loaded_units[i]);
        }
    }

    spawn_all_timer_units();    // Spawns timerd for .timer files

    socket_activation_start(event, loaded_units, unit_count);	// socket_activation.c

    int ret = event_loop_run();
    socket_activation_stop();
    event_loop_shutdown();
    return ret;
}
