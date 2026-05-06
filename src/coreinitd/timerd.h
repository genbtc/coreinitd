// timerd.h — Timer unit scheduling and service activation
#ifndef COREINITD_TIMERD_H
#define COREINITD_TIMERD_H

#include <systemd/sd-event.h>
#include "unit_loader.h"

/**
 * TimerContext - Metadata for a scheduled timer
 * Stored in userdata to track which timer fired and its recurrence settings
 */
typedef struct {
    Unit *timer_unit;
    int boot_sec;        // OnBootSec interval (seconds)
    int active_sec;      // OnUnitActiveSec interval (seconds)
    int has_boot;        // Flag: has valid OnBootSec
    int has_active;      // Flag: has valid OnUnitActiveSec
    int fired_once;      // Flag: timer has fired at least once
} TimerContext;

/**
 * timerd_start() - Initialize and schedule all .timer units
 * @event: sd_event loop to attach timer sources to
 * @units: array of loaded unit structures
 * @count: number of units in the array
 *
 * Scans loaded units for .timer types, parses OnBootSec and OnUnitActiveSec,
 * and registers event loop timers that will trigger associated .service units.
 * Supports both implicit (foo.timer -> foo.service) and explicit (Unit=) linkage.
 * Recurring timers are rescheduled via OnUnitActiveSec; one-shot timers stop after firing.
 *
 * Returns 0 on success, negative errno on failure.
 */
int timerd_start(sd_event *event, Unit *units, size_t count);

/**
 * timerd_stop() - Unregister and clean up all active timers
 * @event: sd_event loop that timers were registered with
 *
 * Unregisters all timer event sources from the event loop,
 * frees associated TimerContext structures, and cleans up
 * internal state. Call during shutdown before event_loop_shutdown().
 *
 * Returns 0 on success, negative errno on failure.
 */
int timerd_stop(sd_event *event);

/**
 * timerd_exit_after_fires() - Test helper for bounded daemon runs
 * @fires: number of timer callbacks after which the event loop exits
 *
 * A value greater than zero asks timerd to call sd_event_exit() after that
 * many timer callbacks. This is useful for CLI smoke tests of recurring
 * OnUnitActiveSec timers without sending an external signal.
 */
void timerd_exit_after_fires(int fires);

extern int parse_sec_to_int(const char *str, int *out);

#endif
