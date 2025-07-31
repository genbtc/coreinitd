#include "timerd.h"
#include "unit_loader.h"
#include "service_manager.h"
#include <systemd/sd-event.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define USEC_PER_SEC 1000000

static Unit *all_units = NULL;
static size_t unit_total = 0;

static int parse_sec_to_int(const char *str, int *out) {
    if (!str || !out) return -1;

    errno = 0;
    char *end = NULL;
    long val = strtol(str, &end, 10);

    if (errno != 0 || end == str || *end != '\0' || val < 0 || val > 86400) {
        // Arbitrary upper limit of 24 hours for sanity
        fprintf(stderr, "[timerd] Invalid time string: '%s'\n", str);
        return -1;
    }

    *out = (int)val;
    return 0;
}

static int on_timer_event(sd_event_source *s, uint64_t usec, void *userdata) {
    Unit *timer_unit = userdata;

    // Derive base name: foo.timer → foo.service
    char base[128];
    strncpy(base, timer_unit->name, sizeof(base));
    char *ext = strstr(base, ".timer");
    if (ext) *ext = '\0';

    for (size_t i = 0; i < unit_total; i++) {
        if (all_units[i].type == UNIT_SERVICE &&
            strncmp(all_units[i].name, base, strlen(base)) == 0) {
            printf("[timerd] Triggering %s from %s\n", all_units[i].name, timer_unit->name);
            service_manager_start(&all_units[i]);
            break;
        }
    }

    return 0;
}

int timerd_start(sd_event *event, Unit *units, size_t count) {
    all_units = units;
    unit_total = count;

    for (size_t i = 0; i < count; i++) {
        Unit *u = &units[i];
        if (u->type != UNIT_TIMER) continue;

        int seconds = 0;
        if (parse_sec_to_int(u->on_boot_sec, &seconds) != 0 || seconds <= 0) {
            fprintf(stderr, "[timerd] Skipping %s (invalid OnBootSec)\n", u->name);
            continue;
        }

        uint64_t now;
        sd_event_now(event, CLOCK_MONOTONIC, &now);
        uint64_t trigger_time = now + ((uint64_t)seconds * USEC_PER_SEC);

        sd_event_source *source;
        int r = sd_event_add_time(event, &source, CLOCK_MONOTONIC, trigger_time,
                                  0, on_timer_event, u);
        if (r < 0) {
            fprintf(stderr, "[timerd] Failed to schedule timer %s: %s\n", u->name, strerror(-r));
            continue;
        }

        printf("[timerd] Scheduled %s to trigger in %d seconds\n", u->name, seconds);
    }

    return 0;
}
