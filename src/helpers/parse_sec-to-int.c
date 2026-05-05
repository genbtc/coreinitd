#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

/**
 * parse_sec_to_int() - Parse a time string to seconds
 * @str: String like "10", "10s", or empty
 * @out: Pointer to store parsed seconds
 *
 * Parses systemd-style time notation. Accepts plain numbers (interpreted as seconds)
 * or numbers with 's' suffix. Returns 0 on success, -1 on error.
 * Rejects negative values and values > 86400 (24 hours).
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

    // Skip optional 's' suffix
    if (*end == 's') end++;

    if (*end != '\0') {
        fprintf(stderr, "[timerd] Invalid time string (trailing junk): '%s'\n", str);
        return -1;
    }

    // Range check: 0-86400 seconds (0-24 hours)
    if (val < 0 || val > 86400) {
        fprintf(stderr, "[timerd] Time value out of range (0-86400s): %ld\n", val);
        return -1;
    }

    *out = (int)val;
    return 0;
}
