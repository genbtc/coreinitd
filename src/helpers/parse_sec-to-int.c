#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>

/**
 * parse_sec_to_int() - Parse a time string to seconds
 * @str: String like "10", "10s", "5m", "1h", or empty
 * @out: Pointer to store parsed seconds
 *
 * Parses a small systemd-style subset. Accepts plain numbers (seconds) or a
 * single suffix: s/sec/second(s), m/min/minute(s), h/hr/hour(s), d/day(s).
 * Returns 0 on success, -1 on error. Rejects negative values and values above
 * 86400 seconds (24 hours).
 */
int parse_sec_to_int(const char *str, int *out) {
    if (!str || !out || str[0] == '\0') {
        return -1;
    }

    errno = 0;
    char *end = NULL;
    long val = strtol(str, &end, 10);

    if (errno != 0 || end == str) {
        fprintf(stderr, "[timerd] Invalid time string (parse error): '%s'\n", str);
        return -1;
    }

    while (*end == ' ' || *end == '\t') end++;

    long multiplier = 1;
    if (*end != '\0') {
        if (strcmp(end, "s") == 0 || strcmp(end, "sec") == 0 ||
            strcmp(end, "second") == 0 || strcmp(end, "seconds") == 0) {
            multiplier = 1;
        } else if (strcmp(end, "m") == 0 || strcmp(end, "min") == 0 ||
                   strcmp(end, "minute") == 0 || strcmp(end, "minutes") == 0) {
            multiplier = 60;
        } else if (strcmp(end, "h") == 0 || strcmp(end, "hr") == 0 ||
                   strcmp(end, "hour") == 0 || strcmp(end, "hours") == 0) {
            multiplier = 60 * 60;
        } else if (strcmp(end, "d") == 0 || strcmp(end, "day") == 0 ||
                   strcmp(end, "days") == 0) {
            multiplier = 24 * 60 * 60;
        } else {
            fprintf(stderr, "[timerd] Invalid time string (unknown suffix): '%s'\n", str);
            return -1;
        }
    }

    if (val < 0 || val > LONG_MAX / multiplier) {
        fprintf(stderr, "[timerd] Time value out of range: %ld\n", val);
        return -1;
    }

    long seconds = val * multiplier;
    if (seconds > 86400) {
        fprintf(stderr, "[timerd] Time value out of range (0-86400s): %ld\n", seconds);
        return -1;
    }

    *out = (int)seconds;
    return 0;
}
