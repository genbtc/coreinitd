#include "config.h"
#include <stdio.h>
#include <string.h>

static int expect_str(const char *label, const char *got, const char *want) {
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "%s: got '%s', want '%s'\n", label, got, want);
        return 1;
    }
    return 0;
}

static int expect_size(const char *label, size_t got, size_t want) {
    if (got != want) {
        fprintf(stderr, "%s: got %zu, want %zu\n", label, got, want);
        return 1;
    }
    return 0;
}

int main(void) {
    const char *path = "build/test-coreinitd.conf";
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("fopen config fixture");
        return 1;
    }

    fputs("# TOML/INI-style flat config\n"
          "[coreinitd]\n"
          "unit_dir = './tmp/units'\n"
          "max_units = 3\n"
          "MAX_SERVICES=4\n"
          "MAX_SOCKETS = 5 # inline comment\n",
          f);
    fclose(f);

    CoreinitdConfig config;
    if (coreinitd_config_load(path, &config) != 0) {
        fprintf(stderr, "Failed to load config fixture\n");
        return 1;
    }

    if (expect_str("unit_dir", config.unit_dir, "./tmp/units") ||
        expect_size("max_units", config.max_units, 3) ||
        expect_size("max_services", config.max_services, 4) ||
        expect_size("max_sockets", config.max_sockets, 5)) {
        return 1;
    }

    coreinitd_config_set_active(&config);
    const CoreinitdConfig *active = coreinitd_config_get();
    if (expect_str("active unit_dir", active->unit_dir, "./tmp/units"))
        return 1;

    printf("Parsed coreinitd runtime config successfully\n");
    return 0;
}
