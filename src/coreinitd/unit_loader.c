#include "unit_loader.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static UnitType infer_unit_type(const char *filename) {
    if (strstr(filename, ".service")) return UNIT_SERVICE;
    if (strstr(filename, ".socket"))  return UNIT_SOCKET;
    if (strstr(filename, ".timer"))   return UNIT_TIMER;
    return UNIT_UNKNOWN;
}

int load_unit(const char *path, Unit *out) {
    memset(out, 0, sizeof(Unit));

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    out->type = infer_unit_type(path);
    strncpy(out->name, path, sizeof(out->name) - 1);

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        // Remove newline
        line[strcspn(line, "\r\n")] = 0;

        // Skip section headers
        if (line[0] == '[') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = 0;
        char *key = line;
        char *val = eq + 1;

        // Strip whitespace (basic)
        while (*key == ' ') key++;
        while (*val == ' ') val++;

        if (strcasecmp(key, "Description") == 0)
            strncpy(out->description, val, sizeof(out->description) - 1);
        else if (strcasecmp(key, "ExecStart") == 0)
            strncpy(out->exec_start, val, sizeof(out->exec_start) - 1);
        else if (strcasecmp(key, "NotifyAccess") == 0)
            strncpy(out->notify_access, val, sizeof(out->notify_access) - 1);
        else if (strcasecmp(key, "Socket") == 0)
            strncpy(out->socket, val, sizeof(out->socket) - 1);
        else if (strcasecmp(key, "Sandbox") == 0)
            out->sandbox = (strcasecmp(val, "true") == 0);
        else if (strcasecmp(key, "OnBootSec") == 0)
            strncpy(out->on_boot_sec, val, sizeof(out->on_boot_sec) - 1);
        else if (strcasecmp(key, "OnUnitActiveSec") == 0)
            strncpy(out->on_active_sec, val, sizeof(out->on_active_sec) - 1);
        else if (strcasecmp(key, "Unit") == 0)
            strncpy(out->timer_unit, val, sizeof(out->timer_unit) - 1);
    }

    fclose(f);
    return 0;
}
