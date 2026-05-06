#include "unit_loader.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

static UnitType infer_unit_type(const char *filename) {
    if (strstr(filename, ".service")) return UNIT_SERVICE;
    if (strstr(filename, ".socket"))  return UNIT_SOCKET;
    if (strstr(filename, ".timer"))   return UNIT_TIMER;
    return UNIT_UNKNOWN;
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;

    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) end--;
    *end = '\0';

    return s;
}

static void copy_value(char *dst, size_t dst_len, const char *val) {
    if (dst_len == 0) return;

    strncpy(dst, val, dst_len - 1);
    dst[dst_len - 1] = '\0';
}

static void append_value(UnitStringList *list, const char *val) {
    if (list->count >= UNIT_LIST_MAX) return;

    copy_value(list->values[list->count], sizeof(list->values[list->count]), val);
    list->count++;
}

static int parse_bool(const char *val) {
    return strcasecmp(val, "yes") == 0 ||
           strcasecmp(val, "true") == 0 ||
           strcasecmp(val, "on") == 0 ||
           strcmp(val, "1") == 0;
}

static UnitSection parse_section(const char *line) {
    if (strcasecmp(line, "[Unit]") == 0) return UNIT_SECTION_UNIT;
    if (strcasecmp(line, "[Service]") == 0) return UNIT_SECTION_SERVICE;
    if (strcasecmp(line, "[Socket]") == 0) return UNIT_SECTION_SOCKET;
    if (strcasecmp(line, "[Install]") == 0) return UNIT_SECTION_INSTALL;
    if (strcasecmp(line, "[Timer]") == 0) return UNIT_SECTION_TIMER;
    return UNIT_SECTION_UNKNOWN;
}

static void parse_unit_verb(Unit *out, const char *key, const char *val) {
    if (strcasecmp(key, "After") == 0)
        append_value(&out->after, val);
    else if (strcasecmp(key, "Description") == 0)
        copy_value(out->description, sizeof(out->description), val);
    else if (strcasecmp(key, "Documentation") == 0)
        append_value(&out->documentation, val);
    else if (strcasecmp(key, "PartOf") == 0)
        append_value(&out->part_of, val);
    else if (strcasecmp(key, "Requires") == 0)
        append_value(&out->requires, val);
    else if (strcasecmp(key, "StartLimitBurst") == 0)
        out->start_limit_burst = atoi(val);
    else if (strcasecmp(key, "StartLimitIntervalSec") == 0)
        copy_value(out->start_limit_interval_sec, sizeof(out->start_limit_interval_sec), val);
}

static void parse_service_verb(Unit *out, const char *key, const char *val) {
    if (strcasecmp(key, "AmbientCapabilities") == 0)
        append_value(&out->ambient_capabilities, val);
    else if (strcasecmp(key, "BusName") == 0)
        copy_value(out->bus_name, sizeof(out->bus_name), val);
    else if (strcasecmp(key, "Environment") == 0)
        append_value(&out->environment, val);
    else if (strcasecmp(key, "ExecReload") == 0)
        copy_value(out->exec_reload, sizeof(out->exec_reload), val);
    else if (strcasecmp(key, "ExecStart") == 0)
        copy_value(out->exec_start, sizeof(out->exec_start), val);
    else if (strcasecmp(key, "ExecStartPost") == 0)
        append_value(&out->exec_start_post, val);
    else if (strcasecmp(key, "KillMode") == 0)
        copy_value(out->kill_mode, sizeof(out->kill_mode), val);
    else if (strcasecmp(key, "MemoryDenyWriteExecute") == 0)
        out->memory_deny_write_execute = parse_bool(val);
    else if (strcasecmp(key, "NoNewPrivileges") == 0)
        out->no_new_privileges = parse_bool(val);
    else if (strcasecmp(key, "Restart") == 0)
        copy_value(out->restart, sizeof(out->restart), val);
    else if (strcasecmp(key, "RestartForceExitStatus") == 0)
        append_value(&out->restart_force_exit_status, val);
    else if (strcasecmp(key, "RestartSec") == 0)
        copy_value(out->restart_sec, sizeof(out->restart_sec), val);
    else if (strcasecmp(key, "Service") == 0)
        copy_value(out->service, sizeof(out->service), val);
    else if (strcasecmp(key, "Slice") == 0)
        copy_value(out->slice, sizeof(out->slice), val);
    else if (strcasecmp(key, "Socket") == 0)
        copy_value(out->socket_unit, sizeof(out->socket_unit), val);
    else if (strcasecmp(key, "SuccessExitStatus") == 0)
        append_value(&out->success_exit_status, val);
    else if (strcasecmp(key, "SystemCallArchitectures") == 0)
        copy_value(out->system_call_architectures, sizeof(out->system_call_architectures), val);
    else if (strcasecmp(key, "TimeoutStopSec") == 0)
        copy_value(out->timeout_stop_sec, sizeof(out->timeout_stop_sec), val);
    else if (strcasecmp(key, "Type") == 0)
        copy_value(out->type_name, sizeof(out->type_name), val);
    else if (strcasecmp(key, "NotifyAccess") == 0)
        copy_value(out->notify_access, sizeof(out->notify_access), val);
    else if (strcasecmp(key, "Sandbox") == 0)
        out->sandbox = parse_bool(val);
}

static void parse_socket_verb(Unit *out, const char *key, const char *val) {
    if (strcasecmp(key, "Accept") == 0)
        out->accept = parse_bool(val);
    else if (strcasecmp(key, "DirectoryMode") == 0)
        copy_value(out->directory_mode, sizeof(out->directory_mode), val);
    else if (strcasecmp(key, "FileDescriptorName") == 0)
        copy_value(out->file_descriptor_name, sizeof(out->file_descriptor_name), val);
    else if (strcasecmp(key, "ListenStream") == 0)
        copy_value(out->listen_stream, sizeof(out->listen_stream), val);
    else if (strcasecmp(key, "Service") == 0)
        copy_value(out->service, sizeof(out->service), val);
    else if (strcasecmp(key, "SocketMode") == 0)
        copy_value(out->socket_mode, sizeof(out->socket_mode), val);
}

static void parse_install_verb(Unit *out, const char *key, const char *val) {
    if (strcasecmp(key, "WantedBy") == 0)
        append_value(&out->wanted_by, val);
}

static void parse_timer_verb(Unit *out, const char *key, const char *val) {
    if (strcasecmp(key, "OnBootSec") == 0)
        copy_value(out->on_boot_sec, sizeof(out->on_boot_sec), val);
    else if (strcasecmp(key, "OnUnitActiveSec") == 0)
        copy_value(out->on_active_sec, sizeof(out->on_active_sec), val);
    else if (strcasecmp(key, "Unit") == 0)
        copy_value(out->timer_unit, sizeof(out->timer_unit), val);
}

static void parse_verb(Unit *out, UnitSection section, const char *key, const char *val) {
    switch (section) {
        case UNIT_SECTION_UNIT:
            parse_unit_verb(out, key, val);
            break;
        case UNIT_SECTION_SERVICE:
            parse_service_verb(out, key, val);
            break;
        case UNIT_SECTION_SOCKET:
            parse_socket_verb(out, key, val);
            break;
        case UNIT_SECTION_INSTALL:
            parse_install_verb(out, key, val);
            break;
        case UNIT_SECTION_TIMER:
            parse_timer_verb(out, key, val);
            break;
        case UNIT_SECTION_NONE:
        case UNIT_SECTION_UNKNOWN:
            // Backward-compatible fallback for existing simple fixture files.
            parse_unit_verb(out, key, val);
            parse_service_verb(out, key, val);
            parse_socket_verb(out, key, val);
            parse_install_verb(out, key, val);
            parse_timer_verb(out, key, val);
            break;
    }
}

int load_unit(const char *path, Unit *out) {
    memset(out, 0, sizeof(Unit));

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    out->type = infer_unit_type(path);
    copy_value(out->name, sizeof(out->name), path);

    UnitSection section = UNIT_SECTION_NONE;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *trimmed = trim(line);

        if (trimmed[0] == '\0' || trimmed[0] == '#' || trimmed[0] == ';') continue;

        if (trimmed[0] == '[') {
            section = parse_section(trimmed);
            out->last_section = section;
            continue;
        }

        char *eq = strchr(trimmed, '=');
        if (!eq) continue;

        *eq = 0;
        char *key = trim(trimmed);
        char *val = trim(eq + 1);

        parse_verb(out, section, key, val);
    }

    fclose(f);
    return 0;
}
