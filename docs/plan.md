# coreinitd Project Plan and Status

## Overview

`coreinitd` is a minimal init and service supervision system. The current implementation is mostly C, with shell scripts and helper binaries used where they make experimentation easier. It links against `libsystemd` or `libelogind` for `sd-event`/low-level compatibility helpers, but the project goal is to avoid depending on the systemd service-manager binary or CLI tools.

## Current Assumptions

- `libsystemd` or `libelogind` is available when building the daemon.
- `bash`/POSIX shell scripts remain useful for bootstrapping and tests, but the daemon path is C-first.
- Logs currently go to stdout/stderr.
- The project is not trying to clone all of systemd; it is targeting a minimal, inspectable subset.

## Finished / Working Components

| Module / Area | Status | Notes |
| --- | --- | --- |
| `main.c` | ✅ Working | Loads runtime config, initializes `sd-event`, loads units, starts non-socket services, starts socket/timer activation, and performs shutdown cleanup. |
| `config.c` / `config.h` | ✅ Working | Reads `etc/coreinitd.conf` or `COREINITD_CONFIG`; supports INI/TOML-style flat `key=value` settings for `unit_dir`, `max_units`, `max_services`, and `max_sockets`. |
| `event_loop.c` | ✅ Working | Wraps the global `sd-event` loop and handles `SIGCHLD`, `SIGINT`, and `SIGTERM`. |
| `unit_loader.c` | ✅ Working | Parses `.service`, `.socket`, and `.timer` unit files and many common systemd-style verbs into `Unit` structs. |
| `service_manager.c` | ✅ Basic | Starts services with `/bin/sh -c`, tracks PIDs in a fixed table capped by runtime config, and stops children during shutdown. |
| `socket_activation.c` | ✅ Basic | Centralized socket activation file; handles UNIX stream sockets and IPv4 `host:port` stream sockets. The old `socket_stream.c` is now duplicate boilerplate and should be deleted once downstream references are removed. |
| `timerd.c` | ✅ Basic | Registers timer events in the main event loop and triggers matching services. |
| Meson tests | ✅ Working in supported environments | `meson test` runs parser tests and the UNIX socket activation smoke test when build dependencies are available. |

## Unfinished / Planned Work

| Area | Status | Next step |
| --- | --- | --- |
| Socket FD passing | ❌ Not implemented | Pass accepted/listening descriptors to services using the `LISTEN_FDS`/`LISTEN_PID` convention instead of accepting and closing clients in the daemon. |
| `Accept=yes` sockets | ❌ Not implemented | Spawn or template per-client service instances. |
| Dependency ordering | 🟡 Parsed only | Enforce `After=`, `Requires=`, `PartOf=`, and related dependency edges before starting units. |
| Restart policies | 🟡 Parsed/basic state only | Implement `Restart=`, `RestartSec=`, success/failure exit classification, and backoff/start-limit behavior. |
| Sandboxing | 🟡 Scaffolding | Wire `Sandbox=true`, capabilities, cgroups, seccomp, namespaces, and `NoNewPrivileges` into the service launch path. |
| D-Bus management | ❌ Not implemented | Add a small management interface if systemd-compatible tooling is a goal. |
| Logging | ❌ Not implemented | Decide between plain log files, syslog, structured stdout/stderr capture, or another minimal backend. |
| Installation paths | 🟡 Experimental | Replace hard-coded install destinations (`/sbin`, `/usr/bin`) with Meson options before packaging. |

## Runtime Configuration Plan

Runtime configuration now replaces several old compile-time settings. The daemon reads `./etc/coreinitd.conf` by default and can be pointed at another file with `COREINITD_CONFIG`.

Supported keys:

```toml
unit_dir = "./etc/units"
max_units = 64
max_services = 64
max_sockets = 64
```

Uppercase aliases (`UNIT_DIR`, `MAX_UNITS`, `MAX_SERVICES`, `MAX_SOCKETS`) are accepted for compatibility with the existing config file. Runtime limits are clamped to the compiled table capacities until the internal arrays are replaced with dynamically allocated storage.

## Socket Activation Plan

Current behavior:

- `.socket` units use `ListenStream=`.
- Absolute paths create UNIX stream sockets.
- `IPv4:port`, `0.0.0.0:port`, `*:port`, and `:port` create IPv4 stream sockets.
- The daemon starts the matching service on activity and accepts/closes the client connection as a placeholder.

Next socket milestones:

1. Delete or stop carrying `src/coreinitd/socket_stream.c` after confirming nothing includes or builds it.
2. Implement `LISTEN_FDS`/`LISTEN_PID` descriptor passing.
3. Preserve accepted client FDs long enough for activated services to consume them.
4. Add `Accept=yes` semantics.
5. Add tests for IPv4 socket activation in addition to the existing UNIX smoke test.

## Build and Test Plan

Primary commands:

```sh
meson setup build
meson compile -C build
meson test -C build
```

Current Meson test coverage:

- `parse-sec`: time string parser.
- `unit-parsing`: `.service`, `.socket`, `.timer`, and verb coverage parsing.
- `config-parsing`: runtime config parser.
- `unix-socket-activation`: daemon smoke test using `/tmp/coreinitd-example.sock`.

## Suggested Next Steps

1. Remove `src/coreinitd/socket_stream.c` after this consolidation lands.
2. Replace static service/socket/unit arrays with dynamically allocated tables so runtime limits are not capped at compile-time capacities.
3. Implement descriptor passing for socket-activated services.
4. Enforce unit dependency ordering.
5. Expand Meson tests for IPv4 sockets and timer-triggered services.
