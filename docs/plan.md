# coreinitd: A Minimal Init and Service Manager using Bash + libsystemd

## Overview

`coreinitd` is a minimal init and service supervision system, built around `bash` and compiled helpers using `libsystemd.so`. It is designed to replicate essential systemd features including socket/timer activation, D-Bus service management, and cgroup-based sandboxing, but without relying on the systemd binary or CLI tools.

## Assumptions

- `libsystemd.so` is available for linking
- `bash` is the available scripting shell
- `logind` is present and functional (for user/session D-Bus access)
- `journald` is not required (logs go to plain files or stdout/stderr)
- The goal is *not* to clone all of systemd, but to support a minimal working alternative

## Core Components

### 1. coreinitd (Main System Daemon)

- Core event loop using `sd-event`
- Launches and tracks services
- Handles child reaping, restart logic, and socket/timer event dispatch
- Acts as the PID 1 (in minimal boot scenarios) or the system orchestrator

### 2. C Helpers (Linked against libsystemd)

- `notify-ready`: wraps `sd_notify("READY=1")`
- `listen-fds`: wraps `sd_listen_fds()` to expose socket file descriptors
- `check-socket`: verifies socket properties via `sd_is_socket_*()`
- `sandbox-launch`: sets up cgroups, seccomp, namespaces
- `my-timerd`: parses `.timer` definitions and schedules launches
- `sd-busd`: manages a limited `org.freedesktop.systemd1` interface over D-Bus

### 3. Bash Components

- `init.sh`: PID 1 launcher or system entrypoint
- `service-launcher.sh`: parses unit files and starts services
- `unitd`: optional top-level bash orchestrator that delegates to helpers

## Unit Format Specification

### .service

```ini
[Unit]
Description=Foo Daemon

[Service]
ExecStart=/usr/bin/foo
NotifyAccess=main
Sandbox=true
Socket=foo.socket
```

### .socket

```ini
[Socket]
ListenStream=12345
Accept=no
```

### .timer

```ini
[Timer]
OnBootSec=10s
OnUnitActiveSec=1h
Unit=foo.service
```

## Implementation Goals

- Fully bootstrap system via `bash` + `coreinitd`
- Avoid reliance on compiled systemd tools (`systemctl`, `journalctl`, etc.)
- Maintain simplicity and transparency of Unix philosophy
- Enable modular unit launch and dynamic configuration loading

## Directory Layout

```
coreinitd/
├── coreinitd/                # C implementation of the main system daemon
├── helpers/                  # C wrappers for libsystemd functions
│   ├── notify-ready.c
│   ├── listen-fds.c
│   ├── check-socket.c
│   ├── sandbox-launch.c
│   ├── my-timerd.c
│   └── sd-busd.c
├── scripts/                  # Bash orchestration scripts
│   ├── init.sh
│   ├── service-launcher.sh
│   └── unitd
├── units/                    # .service, .socket, .timer files
├── docs/                     # Design docs and documentation
│   └── plan.md               # Initial architectural plan
└── meson.build               # Build system for helpers and coreinitd
```

## Future Considerations

- Add support for graphical targets
- Introduce optional `dbus-broker` compatibility
- Explore logging integration via `syslog` or structured stdout capture

---

This document serves as the initial technical direction and architectural plan for the `coreinitd` project.

