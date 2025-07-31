// listen_fds.c
// Lists file descriptors passed by socket activation

#include <systemd/sd-daemon.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(void) {
    int fd_count = sd_listen_fds(0);
    if (fd_count < 0) {
        perror("sd_listen_fds");
        return 1;
    }

    if (fd_count == 0) {
        printf("No file descriptors passed\n");
        return 0;
    }

    for (int i = 0; i < fd_count; ++i) {
        int fd = SD_LISTEN_FDS_START + i;
        printf("FD %d (index %d)\n", fd, i);
        // You can enhance this later with sd_is_socket()
    }

    return 0;
}
