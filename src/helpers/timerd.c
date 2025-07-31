// timerd.c
// Naive .timer parser that schedules service after OnBootSec

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int parse_seconds(const char *s) {
    // Parse simple seconds from strings like "10s"
    int len = strlen(s);
    if (len > 1 && s[len - 1] == 's') {
        char buf[32];
        strncpy(buf, s, len - 1);
        buf[len - 1] = '\0';
        return atoi(buf);
    }
    return atoi(s);  // fallback
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s path/to/unit.timer\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror("fopen");
        return 1;
    }

    char line[256];
    int delay = 0;
    char service_unit[128] = {0};

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "OnBootSec=", 10) == 0) {
            delay = parse_seconds(line + 10);
        } else if (strncmp(line, "Unit=", 5) == 0) {
            strncpy(service_unit, line + 5, sizeof(service_unit) - 1);
            service_unit[strcspn(service_unit, "\r\n")] = '\0'; // trim newline
        }
    }

    fclose(f);

    if (delay > 0) {
        sleep(delay);
    }

    if (strlen(service_unit) > 0) {
        char path[256];
        snprintf(path, sizeof(path), "./scripts/start-unit.sh %s", service_unit);
        system(path);  // replace with exec() or IPC later
    }

    return 0;
}
