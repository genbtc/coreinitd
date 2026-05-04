// event_loop.h — interface for coreinitd's event loop system
#ifndef COREINITD_EVENT_LOOP_H
#define COREINITD_EVENT_LOOP_H

#include <systemd/sd-event.h>

// Global event loop pointer accessible to all modules
extern sd_event *event;

int event_loop_init(void);
int event_loop_run(void);
void event_loop_shutdown(void);

#endif