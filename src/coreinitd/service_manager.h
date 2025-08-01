#ifndef COREINITD_SERVICE_MANAGER_H
#define COREINITD_SERVICE_MANAGER_H

#include <sys/types.h>
#include "unit_loader.h"

typedef enum {
    SERVICE_INACTIVE,
    SERVICE_STARTING,
    SERVICE_ACTIVE,
    SERVICE_FAILED
} ServiceState;

typedef struct {
    Unit *unit;
    pid_t pid;
    ServiceState state;
} ServiceEntry;

int service_manager_start(Unit *unit);
void service_manager_reap(pid_t pid);
void service_manager_status(void);

#endif
