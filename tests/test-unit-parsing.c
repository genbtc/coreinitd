#include "../src/coreinitd/unit_loader.h"
#include <stdio.h>

int main() {
    Unit u;
    if (load_unit("etc/units/example.service", &u) != 0) {
        fprintf(stderr, "Failed to load unit\n");
        return 1;
    }

    printf("Name: %s\nExecStart: %s\nNotify: %s\n",
        u.name, u.exec_start, u.notify_access);

    return 0;
}
