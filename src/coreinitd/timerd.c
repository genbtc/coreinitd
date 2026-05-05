// timerd.c — Timer unit scheduling and service activation
// Integrated into the main sd-event loop (not a separate daemon)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include "timerd.h"
#include "service_manager.h"

#define USEC_PER_SEC 1000000

static Unit *all_units = NULL;
static size_t unit_total = 0;

/**
 * find_service_for_timer() - Locate the service unit triggered by a timer
 * @timer_unit: The .timer unit to look up a service for
 * @units: Array of all loaded units
 * @count: Number of units
 *
 * Tries explicit Unit= first, then falls back to implicit basename matching.
 * For "foo.timer", looks for "foo.service".
 * Returns pointer to matching service unit, or NULL if not found.
 */
static Unit *find_service_for_timer(Unit *timer_unit, Unit *units, size_t count) {
    if (!timer_unit) return NULL;

    // Try explicit Unit= reference first
    if (strlen(timer_unit->timer_unit) > 0) {
        for (size_t i = 0; i < count; i++) {
            if (units[i].type == UNIT_SERVICE &&
                strncmp(units[i].name, timer_unit->timer_unit, 
                       strlen(timer_unit->timer_unit)) == 0) {
                return &units[i];
            }
        }
    }

    // Fall back to implicit basename matching: "foo.timer" -> "foo.service"
    char base[128];
    strncpy(base, timer_unit->name, sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';

    char *ext = strstr(base, ".timer");
    if (ext) *ext = '\0';

    for (size_t i = 0; i < count; i++) {
        if (units[i].type == UNIT_SERVICE) {
            char service_base[128];
            strncpy(service_base, units[i].name, sizeof(service_base) - 1);
            service_base[sizeof(service_base) - 1] = '\0';
            
            char *svc_ext = strstr(service_base, ".service");
            if (svc_ext) *svc_ext = '\0';

            if (strncmp(service_base, base, strlen(base)) == 0) {
                return &units[i];
            }
        }
    }

    return NULL;
}

/**
 * on_timer_event() - sd_event callback when a timer expires
 * @s: The sd_event_source that fired
 * @usec: Current monotonic time in microseconds
 * @userdata: TimerContext pointer (cast from void*)
 *
 * Called when a timer fires. Triggers the associated service and,
 * if OnUnitActiveSec is set, reschedules the timer for the next interval.
 * Returns 0 to keep the timer active (for recurrent timers), or exits callback.
 */
static int on_timer_event(sd_event_source *s, uint64_t usec, void *userdata) {
    TimerContext *ctx = (TimerContext *)userdata;
    Unit *timer_unit = ctx->timer_unit;

    fprintf(stderr, "[timerd] Timer fired: %s\n", timer_unit->name);

    // Find and trigger associated service
    Unit *service = find_service_for_timer(timer_unit, all_units, unit_total);
    if (service) {
        fprintf(stderr, "[timerd] Triggering service %s from timer %s\n",
                service->name, timer_unit->name);
        service_manager_start(service);
        ctx->fired_once = 1;
    } else {
        fprintf(stderr, "[timerd] Warning: No matching service found for timer %s\n",
                timer_unit->name);
    }

    // Handle recurrence with OnUnitActiveSec
    if (ctx->has_active && ctx->active_sec > 0) {
        uint64_t now;
        int r = sd_event_now(sd_event_source_get_event(s), CLOCK_MONOTONIC, &now);
        if (r < 0) {
            fprintf(stderr, "[timerd] Failed to get current time: %s\n", strerror(-r));
            return -1;
        }

        uint64_t next_time = now + (uint64_t)ctx->active_sec * USEC_PER_SEC;
        r = sd_event_source_set_time(s, next_time);
        if (r < 0) {
            fprintf(stderr, "[timerd] Failed to reschedule timer %s: %s\n",
                    timer_unit->name, strerror(-r));
            return -1;
        }

        fprintf(stderr, "[timerd] Rescheduled %s for %" PRIu64 " usec from now\n",
                timer_unit->name, next_time - now);
        return 0;  // Keep timer active for next interval
    }

    // No recurrence via OnUnitActiveSec; timer runs once and stops
    fprintf(stderr, "[timerd] Timer %s will not recur (no OnUnitActiveSec)\n",
            timer_unit->name);
    return 0;
}

/**
 * timerd_start() - Initialize and schedule all .timer units
 * See header file for full documentation.
 */
int timerd_start(sd_event *event, Unit *units, size_t count) {
    if (!event || !units || count == 0) {
        fprintf(stderr, "[timerd] Invalid arguments to timerd_start\n");
        return -1;
    }

    all_units = units;
    unit_total = count;

    int timer_count = 0;

    for (size_t i = 0; i < count; i++) {
        Unit *u = &units[i];
        if (u->type != UNIT_TIMER) continue;

        // Parse timer intervals
        int boot_sec = 0, active_sec = 0;
        int has_boot = (parse_sec_to_int(u->on_boot_sec, &boot_sec) == 0 && boot_sec > 0);
        int has_active = (parse_sec_to_int(u->on_active_sec, &active_sec) == 0 && active_sec > 0);

        if (!has_boot && !has_active) {
            fprintf(stderr, "[timerd] Skipping %s (no valid OnBootSec or OnUnitActiveSec)\n",
                    u->name);
            continue;
        }

        // Validate that the associated service exists
        Unit *service = find_service_for_timer(u, units, count);
        if (!service) {
            fprintf(stderr, "[timerd] Warning: %s has no associated service unit\n", u->name);
            // Still proceed; the service might be added later
        }

        // Create timer context (freed implicitly when event source is destroyed)
        TimerContext *ctx = malloc(sizeof(TimerContext));
        if (!ctx) {
            fprintf(stderr, "[timerd] Failed to allocate timer context\n");
            return -1;
        }

        ctx->timer_unit = u;
        ctx->boot_sec = boot_sec;
        ctx->active_sec = active_sec;
        ctx->has_boot = has_boot;
        ctx->has_active = has_active;
        ctx->fired_once = 0;

        // Get current monotonic time
        uint64_t now;
        int r = sd_event_now(event, CLOCK_MONOTONIC, &now);
        if (r < 0) {
            fprintf(stderr, "[timerd] Failed to get current time: %s\n", strerror(-r));
            free(ctx);
            return -1;
        }

        // Calculate trigger time
        uint64_t trigger_time = now;
        if (has_boot) {
            trigger_time += (uint64_t)boot_sec * USEC_PER_SEC;
            fprintf(stderr, "[timerd] Timer %s: OnBootSec=%d\n", u->name, boot_sec);
        } else if (has_active) {
            // No OnBootSec, but has OnUnitActiveSec: start first interval now
            trigger_time += (uint64_t)active_sec * USEC_PER_SEC;
            fprintf(stderr, "[timerd] Timer %s: OnUnitActiveSec=%d (no OnBootSec)\n",
                    u->name, active_sec);
        }

        // Register timer with sd_event
        sd_event_source *source = NULL;
        r = sd_event_add_time(event, &source, CLOCK_MONOTONIC, trigger_time,
                              0, on_timer_event, ctx);
        if (r < 0) {
            fprintf(stderr, "[timerd] Failed to schedule timer %s: %s\n",
                    u->name, strerror(-r));
            free(ctx);
            continue;
        }

        fprintf(stderr, "[timerd] Scheduled %s to trigger in %" PRIu64 " usec\n",
                u->name, trigger_time - now);
        timer_count++;
    }

    fprintf(stderr, "[timerd] Initialized %d timer unit(s)\n", timer_count);
    return 0;
}