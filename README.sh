#!/bin/bash
# Create README.md
cat > README.md <<'EOF'
# coreinitd

A minimal init and service manager built using Bash and libsystemd.

---

## Overview

`coreinitd` is designed to provide a lightweight alternative to systemd, using bash scripts for orchestration and small C helpers linked against `libsystemd` for low-level system integration. It supports essential features such as socket and timer activation, service supervision, cgroups-based sandboxing, and D-Bus unit management.

## Project Structure

- `src/coreinitd/`: Core daemon source code.
- `src/helpers/`: Small C helper programs that wrap libsystemd functionality.
- `scripts/`: Shell scripts for bootstrapping and service management.
- `etc/units/`: Unit configuration files (.service, .socket, .timer).
- `docs/`: Documentation and project plans.
- `tests/`: Test scripts and helpers.

## Getting Started

1. Build the helpers and core daemon.
2. Place your unit files in `etc/units/`.
3. Use `init.sh` as the system's init or for testing in containers.

## Goals

- Provide a transparent and maintainable init system.
- Minimize dependencies on external binaries.
- Enable extensibility through modular helpers.

## License

TBD

## Contact

Project maintainer: [Your Name]

---
EOF
