#ifndef COREINITD_SOCKET_ACTIVATION_H
#define COREINITD_SOCKET_ACTIVATION_H

int socket_activation_start(sd_event *event, Unit *units, size_t unit_count);
void socket_activation_stop(void);

#endif
