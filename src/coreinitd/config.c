#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static CoreinitdConfig active_config;
static int active_config_initialized = 0;

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

static void strip_inline_comment(char *s) {
    int quote = 0;
    for (; *s; s++) {
        if (*s == '"' || *s == '\'') {
            if (quote == 0) quote = *s;
            else if (quote == *s) quote = 0;
            continue;
        }
        if (quote == 0 && (*s == '#' || *s == ';')) {
            *s = '\0';
            return;
        }
    }
}

static void unquote(char *s) {
    size_t len = strlen(s);
    if (len >= 2 && ((s[0] == '"' && s[len - 1] == '"') ||
                     (s[0] == '\'' && s[len - 1] == '\''))) {
        memmove(s, s + 1, len - 2);
        s[len - 2] = '\0';
    }
}

static size_t clamp_limit(const char *key, unsigned long value, size_t capacity) {
    if (value == 0) {
        fprintf(stderr, "[coreinitd-config] %s must be greater than zero; using 1\n", key);
        return 1;
    }
    if (value > capacity) {
        fprintf(stderr, "[coreinitd-config] %s=%lu exceeds compiled capacity %zu; clamping\n",
                key, value, capacity);
        return capacity;
    }
    return (size_t)value;
}

void coreinitd_config_defaults(CoreinitdConfig *config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
    snprintf(config->unit_dir, sizeof(config->unit_dir), "%s", COREINITD_DEFAULT_UNIT_DIR);
    config->max_units = COREINITD_MAX_UNITS_CAPACITY;
    config->max_services = COREINITD_MAX_SERVICES_CAPACITY;
    config->max_sockets = COREINITD_MAX_SOCKETS_CAPACITY;
}

const char *coreinitd_config_path(void) {
    const char *path = getenv("COREINITD_CONFIG");
    return (path && path[0]) ? path : COREINITD_DEFAULT_CONFIG_PATH;
}

int coreinitd_config_load(const char *path, CoreinitdConfig *config) {
    if (!config) return -1;

    coreinitd_config_defaults(config);
    if (!path || !path[0]) path = coreinitd_config_path();

    FILE *f = fopen(path, "r");
    if (!f) {
        if (errno == ENOENT) {
            fprintf(stderr, "[coreinitd-config] %s not found; using defaults\n", path);
            return 0;
        }
        fprintf(stderr, "[coreinitd-config] Failed to open %s: %s\n", path, strerror(errno));
        return -1;
    }

    char line[512];
    unsigned long lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;
        line[strcspn(line, "\r\n")] = '\0';
        char *entry = trim(line);

        if (entry[0] == '\0' || entry[0] == '#' || entry[0] == ';') continue;
        if (entry[0] == '[') continue;

        strip_inline_comment(entry);
        entry = trim(entry);
        if (entry[0] == '\0') continue;

        char *eq = strchr(entry, '=');
        if (!eq) {
            fprintf(stderr, "[coreinitd-config] Ignoring %s:%lu without '='\n", path, lineno);
            continue;
        }

        *eq = '\0';
        char *key = trim(entry);
        char *val = trim(eq + 1);
        unquote(val);

        if (strcasecmp(key, "UNIT_DIR") == 0 || strcasecmp(key, "unit_dir") == 0) {
            snprintf(config->unit_dir, sizeof(config->unit_dir), "%s", val);
        } else if (strcasecmp(key, "MAX_UNITS") == 0 || strcasecmp(key, "max_units") == 0) {
            config->max_units = clamp_limit(key, strtoul(val, NULL, 10), COREINITD_MAX_UNITS_CAPACITY);
        } else if (strcasecmp(key, "MAX_SERVICES") == 0 || strcasecmp(key, "max_services") == 0) {
            config->max_services = clamp_limit(key, strtoul(val, NULL, 10), COREINITD_MAX_SERVICES_CAPACITY);
        } else if (strcasecmp(key, "MAX_SOCKETS") == 0 || strcasecmp(key, "max_sockets") == 0) {
            config->max_sockets = clamp_limit(key, strtoul(val, NULL, 10), COREINITD_MAX_SOCKETS_CAPACITY);
        } else {
            fprintf(stderr, "[coreinitd-config] Ignoring unknown key %s at %s:%lu\n", key, path, lineno);
        }
    }

    fclose(f);
    return 0;
}

void coreinitd_config_set_active(const CoreinitdConfig *config) {
    if (config) active_config = *config;
    else coreinitd_config_defaults(&active_config);
    active_config_initialized = 1;
}

const CoreinitdConfig *coreinitd_config_get(void) {
    if (!active_config_initialized) {
        coreinitd_config_defaults(&active_config);
        active_config_initialized = 1;
    }
    return &active_config;
}
