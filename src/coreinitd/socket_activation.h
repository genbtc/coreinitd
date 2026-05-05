#ifndef COREINITD_SOCKET_ACTIVATION_H
#define COREINITD_SOCKET_ACTIVATION_H

#include <systemd/sd-event.h>
#include "unit_loader.h"

typedef struct {
    int fd;
    Unit *unit;
    sd_event_source *event_source;
} SocketActivation;

#define MAX_SOCKETS 64
static SocketActivation sockets[MAX_SOCKETS];
static size_t socket_count = 0;

int socket_activation_start(sd_event *event, Unit *units, size_t unit_count);
void socket_activation_stop(void);

#endif
