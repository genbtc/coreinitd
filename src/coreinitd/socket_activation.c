#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <systemd/sd-event.h>
#include "unit_loader.h"
#include "service_manager.h"
#include "socket_activation.h"

static Unit *all_units = NULL;
static size_t unit_total = 0;

static const char *unit_basename(const char *name) {
    const char *slash = strrchr(name, '/');
    return slash ? slash + 1 : name;
}

static Unit *find_matching_service(const Unit *socket_unit, Unit *units, size_t count) {
    char expected[128];
    const char *socket_name = unit_basename(socket_unit->name);
    snprintf(expected, sizeof(expected), "%s", socket_name);

    char *ext = strstr(expected, ".socket");
    if (ext) *ext = '\0';
    strncat(expected, ".service", sizeof(expected) - strlen(expected) - 1);

    fprintf(stderr, "[socket_activation] Looking for service %s for socket %s\n",
            expected, socket_name);

    for (size_t i = 0; i < count; i++) {
        if (units[i].type != UNIT_SERVICE)
            continue;

        const char *service_name = unit_basename(units[i].name);
        if (strcmp(service_name, expected) == 0) {
            fprintf(stderr, "[socket_activation] Matched socket %s to service %s\n",
                    socket_name, service_name);
            return &units[i];
        }
    }
    return NULL;
}

static int make_socket_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int is_unix_path(const char *listen_stream) {
    return listen_stream && listen_stream[0] == '/';
}

static int validate_unix_socket_path(Unit *u) {
    size_t len = strlen(u->listen_stream);

    if (!is_unix_path(u->listen_stream)) {
        fprintf(stderr,
                "[socket_activation] %s ListenStream='%s' is not a UNIX socket path; "
                "use an absolute path such as /tmp/%s.sock\n",
                u->name, u->listen_stream, unit_basename(u->name));
        return -1;
    }

    if (len >= sizeof(((struct sockaddr_un *)0)->sun_path)) {
        fprintf(stderr,
                "[socket_activation] %s ListenStream path is too long (%zu >= %zu): %s\n",
                u->name, len, sizeof(((struct sockaddr_un *)0)->sun_path), u->listen_stream);
        return -1;
    }

    return 0;
}

static int on_socket_event(sd_event_source *s, int fd, uint32_t revents, void *userdata) {
    (void)s;
    SocketActivation *sa = userdata;

    fprintf(stderr, "[socket_activation] Event on %s fd=%d revents=0x%x\n",
            sa && sa->unit ? sa->unit->name : "<unknown>", fd, revents);

    if (revents & (EPOLLERR | EPOLLHUP)) {
        fprintf(stderr, "[socket_activation] Error/hangup event on %s (revents=0x%x)\n",
                sa->unit->name, revents);
    }

    if (revents & (EPOLLIN | EPOLLPRI)) {
        Unit *service = find_matching_service(sa->unit, all_units, unit_total);
        if (service) {
            fprintf(stderr, "[socket_activation] Activating service %s for socket %s\n",
                    service->name, sa->unit->name);
            service_manager_start(service);
        } else {
            fprintf(stderr, "[socket_activation] No matching service for socket %s\n", sa->unit->name);
        }

        for (;;) {
            struct sockaddr_un client_addr;
            socklen_t addrlen = sizeof(client_addr);
            int client_fd = accept4(fd, (struct sockaddr *)&client_addr, &addrlen,
                                    SOCK_CLOEXEC | SOCK_NONBLOCK);
            if (client_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                fprintf(stderr, "[socket_activation] accept4 failed for %s: %s\n",
                        sa->unit->name, strerror(errno));
                return 0;
            }

            fprintf(stderr, "[socket_activation] Accepted UNIX connection on %s (client fd=%d)\n",
                    sa->unit->name, client_fd);
            close(client_fd);         // TODO: pass client_fd to service
        }
    }

    return 0;
}

int socket_activation_start(sd_event *event, Unit *units, size_t unit_count) {
    if (!event || !units) {
        fprintf(stderr, "[socket_activation] Invalid start arguments: event=%p units=%p count=%zu\n",
                (void *)event, (void *)units, unit_count);
        return -1;
    }

    all_units = units;
    unit_total = unit_count;

    fprintf(stderr, "[socket_activation] Scanning %zu loaded unit(s) for UNIX socket activation\n",
            unit_count);

    for (size_t i = 0; i < unit_count; i++) {
        Unit *u = &units[i];
        if (u->type != UNIT_SOCKET) continue;
        if (socket_count >= MAX_SOCKETS) {
            fprintf(stderr, "[socket_activation] Too many socket units loaded, max %d\n", MAX_SOCKETS);
            break;
        }

        fprintf(stderr, "[socket_activation] Preparing socket unit %s ListenStream='%s' Accept=%s\n",
                u->name, u->listen_stream, u->accept ? "yes" : "no");

        if (strlen(u->listen_stream) == 0) {
            fprintf(stderr, "[socket_activation] Socket unit %s has no ListenStream, skipping\n", u->name);
            continue;
        }

        if (validate_unix_socket_path(u) < 0)
            continue;

        if (unlink(u->listen_stream) == 0) {
            fprintf(stderr, "[socket_activation] Removed stale socket path %s\n", u->listen_stream);
        } else if (errno != ENOENT) {
            fprintf(stderr, "[socket_activation] Failed to remove stale socket path %s: %s\n",
                    u->listen_stream, strerror(errno));
            continue;
        }

        int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        if (fd < 0) {
            fprintf(stderr, "[socket_activation] socket(AF_UNIX) failed for %s: %s\n",
                    u->name, strerror(errno));
            continue;
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, u->listen_stream, sizeof(addr.sun_path) - 1);

        socklen_t addrlen = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path) + 1);
        if (bind(fd, (struct sockaddr *)&addr, addrlen) < 0) {
            fprintf(stderr, "[socket_activation] bind(%s) failed for %s: %s\n",
                    u->listen_stream, u->name, strerror(errno));
            close(fd);
            continue;
        }

        if (listen(fd, SOMAXCONN) < 0) {
            fprintf(stderr, "[socket_activation] listen(%s) failed for %s: %s\n",
                    u->listen_stream, u->name, strerror(errno));
            close(fd);
            unlink(u->listen_stream);
            continue;
        }

        if (make_socket_nonblocking(fd) < 0) {
            fprintf(stderr, "[socket_activation] fcntl(O_NONBLOCK) failed for %s: %s\n",
                    u->name, strerror(errno));
            close(fd);
            unlink(u->listen_stream);
            continue;
        }

        sockets[socket_count].fd = fd;
        sockets[socket_count].unit = u;
        sockets[socket_count].event_source = NULL;

        int r = sd_event_add_io(event, &sockets[socket_count].event_source,
                                fd, EPOLLIN, on_socket_event, &sockets[socket_count]);
        if (r < 0) {
            fprintf(stderr, "[socket_activation] Failed to add socket event source for %s fd=%d: %s\n",
                    u->name, fd, strerror(-r));
            close(fd);
            unlink(u->listen_stream);
            sockets[socket_count].fd = -1;
            sockets[socket_count].unit = NULL;
            continue;
        }

        fprintf(stderr, "[socket_activation] Listening on UNIX socket %s (%s, fd=%d)\n",
                u->listen_stream, u->name, fd);
        socket_count++;
    }

    fprintf(stderr, "[socket_activation] Initialized %zu UNIX socket listener(s)\n", socket_count);
    return 0;
}

void socket_activation_stop(void) {
    fprintf(stderr, "[socket_activation] Stopping %zu socket listener(s)\n", socket_count);

    for (size_t i = 0; i < socket_count; i++) {
        const char *path = sockets[i].unit ? sockets[i].unit->listen_stream : NULL;
        if (sockets[i].event_source)
            sockets[i].event_source = sd_event_source_unref(sockets[i].event_source);
        if (sockets[i].fd >= 0) {
            close(sockets[i].fd);
            sockets[i].fd = -1;
        }
        if (path && path[0]) {
            if (unlink(path) == 0) {
                fprintf(stderr, "[socket_activation] Removed socket path %s\n", path);
            } else if (errno != ENOENT) {
                fprintf(stderr, "[socket_activation] Failed to remove socket path %s: %s\n",
                        path, strerror(errno));
            }
        }
        sockets[i].unit = NULL;
    }
    socket_count = 0;
    all_units = NULL;
    unit_total = 0;
}
