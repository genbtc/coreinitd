#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
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

static Unit *find_matching_service(const Unit *socket_unit, Unit *units, size_t count) {
    char base[128];
    strncpy(base, socket_unit->name, sizeof(base));
    char *ext = strstr(base, ".socket");
    if (ext) *ext = '\0';

    for (size_t i = 0; i < count; i++) {
        if (units[i].type != UNIT_SERVICE)
            continue;

        if (strncmp(units[i].name, base, strlen(base)) == 0)
            return &units[i];
    }
    return NULL;
}

static int make_socket_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static Unit *all_units = NULL;
static size_t unit_total = 0;

static int on_socket_event(sd_event_source *s, int fd, uint32_t revents, void *userdata) {
    SocketActivation *sa = userdata;

    if (revents & (EPOLLIN | EPOLLPRI)) {
		//socket-to-service activation
		Unit *service = find_matching_service(sa->unit, all_units, unit_total);
		if (service) {
		    printf("[socket_activation] Activating service %s for socket %s\n", service->name, sa->unit->name);
		    service_manager_start(service);
		} else {
		    printf("[socket_activation] No matching service for socket %s\n", sa->unit->name);
		}

        struct sockaddr_un client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_fd == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                perror("accept");
            return 0;
        }

        printf("[socket_activation] Accepted connection on %s\n", sa->unit->name);

        close(client_fd);         // TODO: pass client_fd to service
    }

    return 0;
}

int socket_activation_start(sd_event *event, Unit *units, size_t unit_count) {
	all_units = units;
	unit_total = unit_count;

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

        // Remove existing socket file, if any
        unlink(u->listen_stream);

        int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        if (fd < 0) {
            perror("socket");
            continue;
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, u->listen_stream, sizeof(addr.sun_path) - 1);

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

        if (make_socket_nonblocking(fd) < 0) {
            perror("fcntl");
            close(fd);
            continue;
        }

        int r = sd_event_add_io(event, &sockets[socket_count].event_source,
                                fd, EPOLLIN, on_socket_event, &sockets[socket_count]);
        if (r < 0) {
            fprintf(stderr, "Failed to add socket event source: %s\n", strerror(-r));
            close(fd);
            continue;
        }

        sockets[socket_count].fd = fd;
        sockets[socket_count].unit = u;

        printf("[socket_activation] Listening on unix socket %s (%s)\n", u->listen_stream, u->name);
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
