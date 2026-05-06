#!/bin/sh
set -eu

BIN=${COREINITD_BIN:-./build/coreinitd}
MAIN_DIR=$(mktemp -d "${TMPDIR:-/tmp}/coreinitd-main.XXXXXX")
DROPIN_DIR=$(mktemp -d "${TMPDIR:-/tmp}/coreinitd-dropin.XXXXXX")
trap 'rm -rf "$MAIN_DIR" "$DROPIN_DIR"' EXIT INT TERM

cat > "$DROPIN_DIR/dropin-demo.service" <<'EOF_SERVICE'
[Unit]
Description=Drop-in Demo Service
PartOf=graphical-session.target

[Service]
Type=dbus
BusName=org.example.Dropin
ExecStart=/bin/true
ExecReload=/bin/true reload
Slice=session.slice
EOF_SERVICE

output=$("$BIN" --unit-dir "$MAIN_DIR" --drop-in "$DROPIN_DIR" --list-units)

printf '%s\n' "$output" | grep -q "dropin-demo.service"
printf '%s\n' "$output" | grep -q "Drop-in Demo Service"

show=$("$BIN" --unit-dir "$MAIN_DIR" --drop-in "$DROPIN_DIR" --show-unit dropin-demo.service)
printf '%s\n' "$show" | grep -q "PartOf\[0\]=graphical-session.target"
printf '%s\n' "$show" | grep -q "BusName=org.example.Dropin"
printf '%s\n' "$show" | grep -q "ExecStart=/bin/true"
printf '%s\n' "$show" | grep -q "ExecReload=/bin/true reload"
printf '%s\n' "$show" | grep -q "Slice=session.slice"
if printf '%s\n' "$show" | grep -q "^Socket="; then
    echo "show-unit printed a missing Socket field" >&2
    exit 1
fi
if printf '%s\n' "$show" | grep -q "^Sandbox="; then
    echo "show-unit printed a missing Sandbox field" >&2
    exit 1
fi

log=$("$BIN" --unit-dir "$MAIN_DIR" --drop-in "$DROPIN_DIR" --check 2>&1 >/dev/null)
printf '%s\n' "$log" | grep -q "dropin-demo.service → dbus://org.example.Dropin"

echo "drop-in unit directory loaded and inspectable"
