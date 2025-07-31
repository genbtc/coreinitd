// socket_activation.c — manage socket units: create, bind, listen, watch events
#define _GNU_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <systemd/sd-event.h>
#include "unit_loader.h"

typedef struct {
    int fd;
    Unit *unit;
    sd_event_source *event_source;
} SocketActivation;

#define MAX_SOCKETS 32
static SocketActivation sockets[MAX_SOCKETS];
static size_t socket_count = 0;

static int make_socket_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int on_socket_event(sd_event_source *s, int fd, uint32_t revents, void *userdata) {
    SocketActivation *sa = userdata;

    if (revents & (EPOLLIN | EPOLLPRI)) {
        // For now, just print and close accepted socket for demo
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        int client_fd = accept(fd, (struct sockaddr*)&addr, &addrlen);
        if (client_fd == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                perror("accept");
            return 0;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(addr.sin_addr), client_ip, sizeof(client_ip));
        printf("[socket_activation] Accepted connection from %s:%d on %s\n",
               client_ip, ntohs(addr.sin_port), sa->unit->name);

        close(client_fd); // TODO: pass this FD to service
    }

    return 0;
}

// Basic IPv4 only parser for ListenStream format: "0.0.0.0:port"
static int parse_listen_stream(const char *listen_str, struct sockaddr_in *addr) {
    char ip[64];
    int port;
    if (sscanf(listen_str, "%63[^:]:%d", ip, &port) != 2) {
        fprintf(stderr, "Failed to parse ListenStream: %s\n", listen_str);
        return -1;
    }

    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    if (strcmp(ip, "0.0.0.0") == 0)
        addr->sin_addr.s_addr = INADDR_ANY;
    else if (inet_pton(AF_INET, ip, &addr->sin_addr) != 1) {
        fprintf(stderr, "Invalid IP address in ListenStream: %s\n", ip);
        return -1;
    }

    return 0;
}

int socket_activation_start(sd_event *event, Unit *units, size_t unit_count) {
    for (size_t i = 0; i < unit_count; i++) {
        Unit *u = &units[i];
        if (u->type != UNIT_SOCKET) continue;
        if (socket_count >= MAX_SOCKETS) {
            fprintf(stderr, "Too many socket units loaded, max %d\n", MAX_SOCKETS);
            break;
        }

        if (strlen(u->listen_stream) == 0) {
            fprintf(stderr, "[socket_activation] Socket unit %s has no ListenStream, skipping\n", u->name);
            continue;
        }

        struct sockaddr_in addr;
        if (parse_listen_stream(u->listen_stream, &addr) < 0) {
            fprintf(stderr, "[socket_activation] Failed to parse ListenStream for %s\n", u->name);
            continue;
        }

        int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        if (fd < 0) {
            perror("socket");
            continue;
        }

        int yes = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
            perror("setsockopt SO_REUSEADDR");
            close(fd);
            continue;
        }

        if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind");
            close(fd);
            continue;
        }

        if (listen(fd, SOMAXCONN) < 0) {
            perror("listen");
            close(fd);
            continue;
        }

        sockets[socket_count].fd = fd;
        sockets[socket_count].unit = u;

        int r = sd_event_add_io(event, &sockets[socket_count].event_source,
                                fd, EPOLLIN, on_socket_event, &sockets[socket_count]);
        if (r < 0) {
            fprintf(stderr, "Failed to add socket event source: %s\n", strerror(-r));
            close(fd);
            continue;
        }

        printf("[socket_activation] Listening on %s (%s)\n", u->listen_stream, u->name);
        socket_count++;
    }

    return 0;
}

void socket_activation_stop(void) {
    for (size_t i = 0; i < socket_count; i++) {
        if (sockets[i].event_source)
            sd_event_source_unref(sockets[i].event_source);
        if (sockets[i].fd >= 0)
            close(sockets[i].fd);
    }
    socket_count = 0;
}
