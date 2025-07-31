// notify_ready.c
// Notifies the service manager that the unit is READY via sd_notify(3)

#include <systemd/sd-daemon.h>
#include <stdio.h>

int main(void) {
    int r = sd_notify(0, "READY=1");
    if (r < 0) {
        perror("sd_notify failed");
        return 1;
    } else if (r == 0) {
        fprintf(stderr, "No notification socket\n");
        return 2;
    }
    return 0;
}
