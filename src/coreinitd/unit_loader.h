#ifndef COREINITD_UNIT_LOADER_H
#define COREINITD_UNIT_LOADER_H

typedef enum {
    UNIT_SERVICE,
    UNIT_SOCKET,
    UNIT_TIMER,
    UNIT_UNKNOWN
} UnitType;

typedef struct {
    UnitType type;
    char name[128];
    char description[256];

    // For Service units
    char exec_start[256];
    char notify_access[32];
    int sandbox;

    // For Socket units
    char listen_stream[64];
    int accept;

    // For Timer units
    char on_boot_sec[32];
    char on_active_sec[32];
    char timer_unit[128];
} Unit;

int load_unit(const char *path, Unit *out);

#endif
