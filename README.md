# coreinitd

`coreinitd` is a small init/service-manager experiment written in C with a few shell utilities and helper binaries. It uses `sd-event` from `libsystemd`/`libelogind` for the daemon event loop, but it does not shell out to `systemctl` or depend on the systemd service manager binary.

## Current Status

### Finished / working now

- Meson builds the main `coreinitd` daemon and helper binaries.
- `meson test` knows about the C parser tests and the UNIX/IPv4 socket activation smoke tests.
- `.service`, `.socket`, and `.timer` unit files can be loaded from a configured unit directory.
- Runtime configuration is read from `etc/coreinitd.conf` by default, with `COREINITD_CONFIG=/path/to/file` available as an override.
- The unit directory and runtime limits (`unit_dir`/`UNIT_DIR`, `max_units`, `max_services`, `max_sockets`) are configurable in a simple INI/TOML-style `key = value` format.
- Non-socket-activated services are started directly by the service manager.
- Socket activation is centralized in `src/coreinitd/socket_activation.c` and currently handles UNIX stream sockets plus IPv4 `host:port` stream sockets.
- Timers are registered on the daemon event loop and can trigger matching services.

### Unfinished / planned work

- Socket activation still accepts and closes client sockets; full `LISTEN_FDS`/`LISTEN_PID` descriptor passing into activated services is not implemented yet.
- `Accept=yes` per-connection services are parsed but not implemented.
- Dependency ordering (`After=`, `Requires=`, `PartOf=`) is parsed but not enforced.
- Service supervision is minimal; restart policies and full state transitions are future work.
- Sandboxing/cgroups/seccomp helpers are present only as early scaffolding.
- D-Bus/systemd-compatible unit management is not implemented.

## Project Structure

- `src/coreinitd/`: Core daemon source code.
- `src/helpers/`: Small C helper programs.
- `scripts/`: Shell scripts for bootstrapping and service management experiments.
- `etc/coreinitd.conf`: Runtime daemon configuration.
- `etc/units/`: Example unit files (`.service`, `.socket`, `.timer`).
- `docs/`: Design notes and project plans.
- `tests/`: Parser tests and socket activation smoke tests.

## Configuration

`coreinitd` reads `./etc/coreinitd.conf` unless `COREINITD_CONFIG` points somewhere else. The config format is intentionally flat and accepts both INI-style uppercase keys and TOML-style lowercase keys:

```ini
# ./etc/coreinitd.conf
UNIT_DIR="./etc/units"
MAX_UNITS=64
MAX_SERVICES=64
MAX_SOCKETS=64
```

Equivalent lowercase/TOML-style keys are also accepted:

```toml
unit_dir = "./etc/units"
max_units = 64
max_services = 64
max_sockets = 64
```

## Build and Test

```sh
meson setup build
meson compile -C build
meson test -C build
```

The socket activation smoke tests start the built daemon, connect to the example UNIX socket at `/tmp/coreinitd-example.sock` and a temporary IPv4 unit on `127.0.0.1:9999`, and then terminate the daemon.

## License

TBD, most likely GPL.

## Contact

Project maintainer: genBTC
