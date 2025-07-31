# coreinitd Design Document

coreinitd is a minimal, modular init and service supervision system, intended as a replacement for systemd in simple or controlled environments. 
It is implemented primarily in C with a strong preference for simplicity, explicit behavior, and Bash integration where needed.

---

## Goals

- Replace systemd for init, service, and socket activation
- Avoid monolithic design: use discrete C modules
- Use Unix domain sockets instead of IP networking
- Prefer text-based logs (no journald requirement)
- Minimal dependencies (uses `libsystemd` for `sd-event`, optional features)

---

## Component Overview

### 🔁 `main.c`
- Top-level init process (`PID 1`)
- Initializes the `sd-event` loop
- Loads unit files from the filesystem
- Starts services and timers
- Registers signal handlers (e.g. `SIGCHLD`)
- Starts socket activation listener

---

### ⚙️ `event_loop.[c|h]`
- Wraps libsystemd's `sd-event`
- Registers IO and signal handlers
- Provides `event_loop_init()`, `event_loop_run()`, etc.

---

### 📦 `unit_loader.[c|h]`
- Loads `.service`, `.socket`, and `.timer` units
- Parses directives like `ExecStart=`, `ListenStream=`
- Populates global `Unit[]` table
- Does no dependency resolution yet

---

### 🔊 `socket_activation.[c|h]`
- Handles all `.socket` unit logic
- Creates and binds **Unix domain sockets** (not IP)
- Registers them with `sd-event`
- On client connection, triggers associated `.service`
- Will eventually pass FDs using `LISTEN_FDS`

---

### 🔧 `service_manager.[c|h]`
- Starts `.service` units via `fork` and `exec`
- Tracks running processes and status
- Handles reaping via `SIGCHLD`
- Will soon support socket FD passing and sandboxing

---

### ⏰ `timerd.[c|h]` *(stub)*
- Responsible for `.timer` units
- Will schedule `ExecStart=` triggers for associated services
- Currently only placeholder logic

---

## Planned Features

- [x] Service launching with `ExecStart`
- [x] Unix socket activation
- [ ] Pass socket FDs via `LISTEN_FDS` protocol
- [ ] `Accept=yes` behavior (per-connection service forking)
- [ ] Minimal sandboxing (chroot, seccomp, etc.)
- [ ] `.timer` to `.service` integration
- [ ] Unit dependency resolution: `Requires=`, `After=`
- [ ] Reload support via `SIGHUP` or a control API
- [ ] Basic CLI interface for unit management

---

## Unit File Format

Uses a simplified `.ini`-style format similar to systemd units:

### Example `.service`:
[Unit]
Description=My Test Service

[Service]
ExecStart=/usr/bin/my-app
### Example `.socket`:
[Socket]
ListenStream=/run/my-app.sock
### Example `.timer`:
[Timer]
OnBootSec=5

---

## Runtime Directory Layout

| Path | Purpose |
|------|---------|
| `/etc/coreinitd/units/` | All unit files (`*.service`, `*.socket`, etc.) |
| `/run/` | Socket files created by `.socket` units |
| `/tmp/` | Temporary state if needed |
| `/var/log/` | Logs (if used) |

---

## Design Principles

- **Separation of concerns**: each module handles one responsibility
- **Minimal policy**: follow clear triggers, no auto-magic
- **Event-driven**: `sd-event` loop is the core orchestrator
- **No binary logs**: stdout/err redirection to files is sufficient
- **No D-Bus requirement** (but may be integrated later)

---

## Non-Goals

- Re-implement full systemd feature set
- Support arbitrary user sessions or graphical login
- Provide network service supervision (for now)

---

## Contributors

Initial design and architecture by GenBTC, 2025.

