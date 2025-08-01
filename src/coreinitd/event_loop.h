// event_loop.h — interface for coreinitd's event loop system

#ifndef COREINITD_EVENT_LOOP_H
#define COREINITD_EVENT_LOOP_H
#include <systemd/sd-event.h>
static sd_event *event = NULL;

int event_loop_init(void);
int event_loop_run(void);
void event_loop_shutdown(void);

#endif
