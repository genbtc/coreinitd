// main.c — coreinitd unified daemon with event loop + integrated timer handling
//           systemd executor + socket activation
// 2025,2026 (C) genr8eofl - @ gentoo libera IRC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <systemd/sd-event.h>
#include "unit_loader.h"
#include "config.h"

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.12"
#endif

static CoreinitdConfig config;
static Unit loaded_units[COREINITD_MAX_UNITS_CAPACITY];
static size_t unit_count = 0;

typedef struct {
    const char *config_path;
    const char *unit_dir_override;
    const char *drop_in_dir;
    const char *show_unit;
    int list_units;
    int check_only;
    int no_services;
    int no_sockets;
    int no_timers;
    int run_for_sec;
    int timer_fires;
} CoreinitdCliOptions;

#include "service_manager.h"
#include "socket_activation.h"
#include "timerd.h"
#include "event_loop.h"


static const char *unit_type_name(UnitType type) {
    switch (type) {
        case UNIT_SERVICE: return "service";
        case UNIT_SOCKET: return "socket";
        case UNIT_TIMER: return "timer";
        case UNIT_UNKNOWN:
        default: return "unknown";
    }
}

static void print_help(const char *argv0) {
    printf("coreinitd - small systemd-like service/socket/timer daemon\n\n");
    printf("Usage:\n");
    printf("  %s [OPTIONS]\n\n", argv0);
    printf("Core options:\n");
    printf("  -h, --help                 Show this help text and exit\n");
    printf("  -V, --version              Show version and exit\n");
    printf("  -c, --config PATH          Read config from PATH (default: %s or COREINITD_CONFIG)\n", COREINITD_DEFAULT_CONFIG_PATH);
    printf("  -u, --unit-dir DIR         Override the configured unit directory\n");
    printf("      --drop-in DIR          Load extra unit snippets from DIR after the main unit dir\n");
    printf("      --check                Parse configuration and unit files, then exit\n");
    printf("      --list-units           Parse units and print a compact unit table\n");
    printf("      --show-unit UNIT       Print parsed fields for UNIT (name or path)\n\n");
    printf("Daemon/test controls:\n");
    printf("      --no-services          Do not auto-start plain .service units\n");
    printf("      --no-sockets           Do not listen on .socket units\n");
    printf("      --no-timers            Do not schedule .timer units\n");
    printf("      --run-for-sec SEC      Leave the event loop after SEC seconds (test/smoke mode)\n");
    printf("      --timer-fires N        Leave after N timer callbacks (tests OnUnitActiveSec recurrence)\n\n");
    printf("Argument roadmap:\n");
    printf("  start|stop|restart UNIT, status UNIT, cat UNIT, verify UNIT,\n");
    printf("  daemon --foreground, --log-level LEVEL, --pid-file PATH, --state-dir DIR,\n");
    printf("  timer trigger UNIT, timer list, socket list, socket fdinfo UNIT,\n");
    printf("  --user/--system, --root DIR, --set KEY=VALUE, --drop-in DIR, --dry-run.\n");
}

static void print_version(void) {
    printf("coreinitd %s\n", PROJECT_VERSION);
}

static int parse_positive_int(const char *label, const char *value, int *out) {
    char *end = NULL;
    errno = 0;
    long parsed = strtol(value, &end, 10);
    if (errno || end == value || *end != '\0' || parsed <= 0 || parsed > 86400) {
        fprintf(stderr, "[coreinitd-main] Invalid %s: %s\n", label, value);
        return -1;
    }
    *out = (int)parsed;
    return 0;
}

static int parse_cli(int argc, char **argv, CoreinitdCliOptions *opts) {
    memset(opts, 0, sizeof(*opts));

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_help(argv[0]);
            exit(0);
        } else if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0) {
            print_version();
            exit(0);
        } else if (strcmp(arg, "-c") == 0 || strcmp(arg, "--config") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "%s requires a path\n", arg);
                return -1;
            }
            opts->config_path = argv[i];
        } else if (strcmp(arg, "-u") == 0 || strcmp(arg, "--unit-dir") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "%s requires a directory\n", arg);
                return -1;
            }
            opts->unit_dir_override = argv[i];
        } else if (strcmp(arg, "--drop-in") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "%s requires a directory\n", arg);
                return -1;
            }
            opts->drop_in_dir = argv[i];
        } else if (strcmp(arg, "--show-unit") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "%s requires a unit name or path\n", arg);
                return -1;
            }
            opts->show_unit = argv[i];
        } else if (strcmp(arg, "--run-for-sec") == 0) {
            if (++i >= argc || parse_positive_int("--run-for-sec", argv[i], &opts->run_for_sec) < 0)
                return -1;
        } else if (strcmp(arg, "--timer-fires") == 0) {
            if (++i >= argc || parse_positive_int("--timer-fires", argv[i], &opts->timer_fires) < 0)
                return -1;
        } else if (strcmp(arg, "--check") == 0) {
            opts->check_only = 1;
        } else if (strcmp(arg, "--list-units") == 0) {
            opts->list_units = 1;
        } else if (strcmp(arg, "--no-services") == 0) {
            opts->no_services = 1;
        } else if (strcmp(arg, "--no-sockets") == 0) {
            opts->no_sockets = 1;
        } else if (strcmp(arg, "--no-timers") == 0) {
            opts->no_timers = 1;
        } else {
            fprintf(stderr, "Unknown option: %s\nTry '%s --help'.\n", arg, argv[0]);
            return -1;
        }
    }

    return 0;
}

static int unit_name_matches(const Unit *unit, const char *needle) {
    const char *slash = strrchr(unit->name, '/');
    const char *base = slash ? slash + 1 : unit->name;
    return strcmp(unit->name, needle) == 0 || strcmp(base, needle) == 0;
}

static void print_unit_list(void) {
    printf("TYPE\tNAME\tDESCRIPTION\n");
    for (size_t i = 0; i < unit_count; i++) {
        printf("%s\t%s\t%s\n", unit_type_name(loaded_units[i].type),
               loaded_units[i].name,
               loaded_units[i].description[0] ? loaded_units[i].description : "-");
    }
}

static void print_string_list(const char *label, const UnitStringList *list) {
    for (size_t i = 0; i < list->count; i++)
        printf("  %s[%zu]=%s\n", label, i, list->values[i]);
}

static int print_unit_details(const char *needle) {
    for (size_t i = 0; i < unit_count; i++) {
        Unit *u = &loaded_units[i];
        if (!unit_name_matches(u, needle)) continue;

        printf("Name=%s\n", u->name);
        printf("Type=%s\n", unit_type_name(u->type));
        printf("Description=%s\n", u->description);
        print_string_list("After", &u->after);
        print_string_list("Documentation", &u->documentation);
        print_string_list("Requires", &u->requires);

        if (u->type == UNIT_SERVICE) {
            printf("ExecStart=%s\n", u->exec_start);
            printf("TypeName=%s\n", u->type_name);
            printf("NotifyAccess=%s\n", u->notify_access);
            printf("Socket=%s\n", u->socket_unit);
            printf("Sandbox=%s\n", u->sandbox ? "true" : "false");
        } else if (u->type == UNIT_SOCKET) {
            printf("ListenStream=%s\n", u->listen_stream);
            printf("Accept=%s\n", u->accept ? "yes" : "no");
            printf("Service=%s\n", u->service);
            printf("FileDescriptorName=%s\n", u->file_descriptor_name);
        } else if (u->type == UNIT_TIMER) {
            printf("OnBootSec=%s\n", u->on_boot_sec);
            printf("OnUnitActiveSec=%s\n", u->on_active_sec);
            printf("Unit=%s\n", u->timer_unit);
        }
        return 0;
    }

    fprintf(stderr, "[coreinitd-main] Unit not found: %s\n", needle);
    return 1;
}

static int on_run_for_timeout(sd_event_source *s, uint64_t usec, void *userdata) {
    (void)usec;
    (void)userdata;
    sd_event *loop = sd_event_source_get_event(s);
    fprintf(stderr, "[coreinitd-main] --run-for-sec timeout reached, leaving event loop\n");
    sd_event_source_unref(s);
    return sd_event_exit(loop, 0);
}

static int schedule_run_for_timeout(int seconds) {
    if (seconds <= 0) return 0;

    uint64_t now;
    int r = sd_event_now(event, CLOCK_MONOTONIC, &now);
    if (r < 0) {
        fprintf(stderr, "[coreinitd-main] Failed to get event time: %s\n", strerror(-r));
        return r;
    }

    sd_event_source *source = NULL;
    r = sd_event_add_time(event, &source, CLOCK_MONOTONIC,
                          now + (uint64_t)seconds * 1000000, 0,
                          on_run_for_timeout, NULL);
    if (r < 0) {
        fprintf(stderr, "[coreinitd-main] Failed to add --run-for-sec timer: %s\n", strerror(-r));
        return r;
    }

    fprintf(stderr, "[coreinitd-main] Will stop after %d second(s)\n", seconds);
    return 0;
}

static int load_units_from_dir(const char *dir, const char *label) {
    DIR *d = opendir(dir);
    if (!d) {
        fprintf(stderr, "[coreinitd] Failed to open %s unit directory %s: %s\n",
                label, dir, strerror(errno));
        return -1;
    }

    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (!(strstr(ent->d_name, ".service") ||
            strstr(ent->d_name, ".socket") ||
            strstr(ent->d_name, ".timer"))) continue;

        if (unit_count >= config.max_units) {
            fprintf(stderr, "[coreinitd] Unit limit reached (%zu)\n", config.max_units);
            break;
        }

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        const char *type_str = "unknown";

        if (load_unit(path, &loaded_units[unit_count]) == 0) {
            switch (loaded_units[unit_count].type) {
                case UNIT_SERVICE: type_str = "service"; break;
                case UNIT_SOCKET: type_str = "socket"; break;
                case UNIT_TIMER: type_str = "timer"; break;
                default: break;
            }
            fprintf(stderr, "[coreinitd] Loaded %s %s unit: %s → %s\n",
                label, type_str, ent->d_name, loaded_units[unit_count].exec_start);
            unit_count++;
        } else {
            fprintf(stderr, "[coreinitd] Failed to load %s unit %s\n", label, ent->d_name);
        }
    }

    closedir(d);
    return 0;
}

static int load_all_units(const char *drop_in_dir) {
    unit_count = 0;

    if (load_units_from_dir(config.unit_dir, "main") < 0)
        return -1;

    if (drop_in_dir && drop_in_dir[0])
        return load_units_from_dir(drop_in_dir, "drop-in");

    return 0;
}

// ─────────────────
// Main Entry Point
// ─────────────────
int main(int argc, char **argv) {
    CoreinitdCliOptions cli;
    if (parse_cli(argc, argv, &cli) < 0)
        return 2;

    fprintf(stderr, "[coreinitd-main] Starting...\n");

    const char *config_path = cli.config_path ? cli.config_path : coreinitd_config_path();
    if (coreinitd_config_load(config_path, &config) < 0)
        return 1;
    if (cli.unit_dir_override)
        snprintf(config.unit_dir, sizeof(config.unit_dir), "%s", cli.unit_dir_override);
    coreinitd_config_set_active(&config);
    fprintf(stderr, "[coreinitd-main] Config: UNIT_DIR=%s MAX_UNITS=%zu MAX_SERVICES=%zu MAX_SOCKETS=%zu\n",
            config.unit_dir, config.max_units, config.max_services, config.max_sockets);

    // Parses and loads .service, .socket, .timer files
    if (load_all_units(cli.drop_in_dir) < 0)
        return 1;

    if (cli.list_units) {
        print_unit_list();
        return 0;
    }

    if (cli.show_unit)
        return print_unit_details(cli.show_unit);

    if (cli.check_only) {
        fprintf(stderr, "[coreinitd-main] Check OK: parsed %zu unit(s) from %s\n",
                unit_count, config.unit_dir);
        return 0;
    }

    if (event_loop_init() < 0)
        return 1;

    if (cli.timer_fires > 0)
        timerd_exit_after_fires(cli.timer_fires);

    // Start services that are not activated by a socket or timer.
    // Socket- and timer-bound services are launched on demand by their activator.
    for (size_t i = 0; i < unit_count; i++) {
        if (loaded_units[i].type != UNIT_SERVICE)
            continue;

        if (strlen(loaded_units[i].socket_unit) > 0) {
            fprintf(stderr, "[coreinitd] Deferring socket-activated service %s (Socket=%s)\n",
                    loaded_units[i].name, loaded_units[i].socket_unit);
            continue;
        }

        if (!cli.no_services)
            service_manager_start(&loaded_units[i]);
    }

    // Initialize socket activation
    if (!cli.no_sockets)
        socket_activation_start(event, loaded_units, unit_count);

    // Initialize timer activation (integrated into main event loop, not external process)
    if (!cli.no_timers)
        timerd_start(event, loaded_units, unit_count);

    if (schedule_run_for_timeout(cli.run_for_sec) < 0)
        return 1;

    int ret = event_loop_run();

    //Stop, Teardown
    service_manager_stop_all();
    timerd_stop(event);
    socket_activation_stop();
    event_loop_shutdown();

    fprintf(stderr, "[coreinitd-main] Stopped All & Shutdown.\n");
    return ret;
}
