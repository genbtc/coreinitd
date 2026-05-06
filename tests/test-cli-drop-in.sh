#!/bin/sh
set -eu

BIN=${COREINITD_BIN:-./build/coreinitd}
MAIN_DIR=$(mktemp -d "${TMPDIR:-/tmp}/coreinitd-main.XXXXXX")
DROPIN_DIR=$(mktemp -d "${TMPDIR:-/tmp}/coreinitd-dropin.XXXXXX")
trap 'rm -rf "$MAIN_DIR" "$DROPIN_DIR"' EXIT INT TERM

cat > "$DROPIN_DIR/dropin-demo.service" <<'EOF_SERVICE'
[Unit]
Description=Drop-in Demo Service

[Service]
ExecStart=/bin/true
EOF_SERVICE

output=$("$BIN" --unit-dir "$MAIN_DIR" --drop-in "$DROPIN_DIR" --list-units)

printf '%s\n' "$output" | grep -q "dropin-demo.service"
printf '%s\n' "$output" | grep -q "Drop-in Demo Service"

show=$("$BIN" --unit-dir "$MAIN_DIR" --drop-in "$DROPIN_DIR" --show-unit dropin-demo.service)
printf '%s\n' "$show" | grep -q "ExecStart=/bin/true"

echo "drop-in unit directory loaded and inspectable"
