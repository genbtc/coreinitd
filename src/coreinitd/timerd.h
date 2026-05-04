// timerd.h — Timer unit scheduling and service activation
#ifndef COREINITD_TIMERD_H
#define COREINITD_TIMERD_H

#include <systemd/sd-event.h>
#include "unit_loader.h"

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

#endif