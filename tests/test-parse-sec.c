#include <stdio.h>

int parse_sec_to_int(const char *str, int *out);

static int expect_seconds(const char *input, int want) {
    int got = -1;
    if (parse_sec_to_int(input, &got) != 0 || got != want) {
        fprintf(stderr, "%s: got %d, want %d\n", input, got, want);
        return 1;
    }
    return 0;
}

int main(void) {
    if (expect_seconds("10", 10) ||
        expect_seconds("10s", 10) ||
        expect_seconds("5m", 300) ||
        expect_seconds("1h", 3600) ||
        expect_seconds("1d", 86400)) {
        return 1;
    }

    int out = 0;
    if (parse_sec_to_int("25h", &out) == 0 ||
        parse_sec_to_int("1fortnight", &out) == 0) {
        fprintf(stderr, "invalid timer values were accepted\n");
        return 1;
    }

    printf("Parsed timer second/minute/hour/day values successfully\n");
    return 0;
}
