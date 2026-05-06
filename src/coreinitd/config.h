#ifndef COREINITD_CONFIG_H
#define COREINITD_CONFIG_H

#include <stddef.h>

#define COREINITD_DEFAULT_CONFIG_PATH "./etc/coreinitd.conf"
#define COREINITD_DEFAULT_UNIT_DIR "./etc/units"
#define COREINITD_MAX_UNITS_CAPACITY 64
#define COREINITD_MAX_SERVICES_CAPACITY 64
#define COREINITD_MAX_SOCKETS_CAPACITY 64

typedef struct {
    char unit_dir[512];
    size_t max_units;
    size_t max_services;
    size_t max_sockets;
} CoreinitdConfig;

void coreinitd_config_defaults(CoreinitdConfig *config);
int coreinitd_config_load(const char *path, CoreinitdConfig *config);
const char *coreinitd_config_path(void);
const CoreinitdConfig *coreinitd_config_get(void);
void coreinitd_config_set_active(const CoreinitdConfig *config);

#endif
