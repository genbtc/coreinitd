// get_unit_info.c
// Simple dump of unit file content

#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s unit-file\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror("fopen");
        return 1;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        fputs(line, stdout);
    }

    fclose(f);
    return 0;
}
