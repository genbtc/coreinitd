#ifndef COREINITD_SOCKET_ACTIVATION_H
#define COREINITD_SOCKET_ACTIVATION_H

#include <systemd/sd-event.h>
#include "unit_loader.h"

typedef enum {
    SOCKET_KIND_UNIX_STREAM,
    SOCKET_KIND_INET_STREAM
} SocketActivationKind;

typedef struct {
    int fd;
    Unit *unit;
    SocketActivationKind kind;
    sd_event_source *event_source;
} SocketActivation;

int socket_activation_start(sd_event *event, Unit *units, size_t unit_count);
void socket_activation_stop(void);

#endif
