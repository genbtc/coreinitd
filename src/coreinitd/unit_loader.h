#ifndef COREINITD_UNIT_LOADER_H
#define COREINITD_UNIT_LOADER_H

#include <stddef.h>

#define UNIT_VALUE_LEN 256
#define UNIT_LIST_MAX 16

typedef enum {
    UNIT_SERVICE,
    UNIT_SOCKET,
    UNIT_TIMER,
    UNIT_UNKNOWN
} UnitType;

typedef enum {
    UNIT_SECTION_NONE,
    UNIT_SECTION_UNIT,
    UNIT_SECTION_SERVICE,
    UNIT_SECTION_SOCKET,
    UNIT_SECTION_INSTALL,
    UNIT_SECTION_TIMER,
    UNIT_SECTION_UNKNOWN
} UnitSection;

typedef struct {
    char values[UNIT_LIST_MAX][UNIT_VALUE_LEN];
    size_t count;
} UnitStringList;

typedef struct {
    UnitType type;
    UnitSection last_section;
    char name[128];
    char description[256];

    // [Unit] metadata and ordering/dependency verbs
    UnitStringList after;
    UnitStringList documentation;
    UnitStringList part_of;
    UnitStringList requires;
    int start_limit_burst;
    char start_limit_interval_sec[32];

    // [Service] verbs
    char exec_reload[256];
    char exec_start[256];
    UnitStringList exec_start_post;
    char notify_access[32];
    int sandbox;
    int sandbox_set;
    char type_name[32];
    char bus_name[128];
    UnitStringList environment;
    UnitStringList ambient_capabilities;
    char kill_mode[32];
    char restart[32];
    UnitStringList restart_force_exit_status;
    char restart_sec[32];
    UnitStringList success_exit_status;
    char timeout_stop_sec[32];
    char slice[128];
    int memory_deny_write_execute;
    int memory_deny_write_execute_set;
    int no_new_privileges;
    int no_new_privileges_set;
    char system_call_architectures[64];

    // For Socket units
    char listen_stream[64];	// Unix path, TCP port, etc.
    int accept;		// For Accept=yes|no
    int accept_set;
    char directory_mode[16];
    char file_descriptor_name[64];
    char service[128];
    char socket_mode[16];

    // For Timer units
    char on_boot_sec[32];
    char on_active_sec[32];
    char timer_unit[128];

    // [Install] verbs
    UnitStringList wanted_by;

	// Service->Socket Activation (if `.socket` is a reference to another unit)
    char socket_unit[128];     // Name of .socket unit linked from a .service
} Unit;

int load_unit(const char *path, Unit *out);

#endif
