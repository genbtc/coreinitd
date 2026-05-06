#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

TCP_HOST=127.0.0.1
TCP_PORT=9999
LOG_FILE=$(mktemp)
TMP_DIR=$(mktemp -d)
UNIT_DIR="$TMP_DIR/units"
CONFIG_FILE="$TMP_DIR/coreinitd.conf"
PID=

cleanup() {
    if [ -n "${PID:-}" ] && kill -0 "$PID" 2>/dev/null; then
        kill -TERM "$PID" 2>/dev/null || true
        wait "$PID" 2>/dev/null || true
    fi
    rm -rf "$TMP_DIR"
    rm -f "$LOG_FILE"
}
trap cleanup EXIT INT TERM

COREINITD_BIN=${COREINITD_BIN:-./build/coreinitd}
if [ ! -x "$COREINITD_BIN" ]; then
    echo "$COREINITD_BIN is missing; build with: meson setup build && meson compile -C build" >&2
    exit 77
fi

python3 - <<'PY'
import socket
import sys

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
try:
    sock.bind(('127.0.0.1', 9999))
except OSError as exc:
    print(f'TCP port 9999 is unavailable: {exc}', file=sys.stderr)
    sys.exit(77)
finally:
    sock.close()
PY

mkdir -p "$UNIT_DIR"
cat >"$UNIT_DIR/ipv4-example.socket" <<EOF_SOCKET
[Socket]
ListenStream=$TCP_HOST:$TCP_PORT
Accept=no
EOF_SOCKET

cat >"$UNIT_DIR/ipv4-example.service" <<'EOF_SERVICE'
[Unit]
Description=IPv4 Socket Activation Smoke Service

[Service]
ExecStart=/bin/true
Socket=ipv4-example.socket
EOF_SERVICE

cat >"$CONFIG_FILE" <<EOF_CONFIG
unit_dir = "$UNIT_DIR"
max_units = 8
max_services = 8
max_sockets = 8
EOF_CONFIG

COREINITD_CONFIG="$CONFIG_FILE" "$COREINITD_BIN" >"$LOG_FILE" 2>&1 &
PID=$!

python3 - <<'PY'
import socket
import sys
import time

last_error = None
for _ in range(50):
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client.settimeout(0.2)
    try:
        client.connect(('127.0.0.1', 9999))
        client.close()
        break
    except OSError as exc:
        last_error = exc
        client.close()
        time.sleep(0.1)
else:
    print(f'IPv4 socket on 127.0.0.1:9999 was not reachable: {last_error}', file=sys.stderr)
    sys.exit(1)
PY

sleep 1
kill -TERM "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true
PID=

grep -q "Listening on IPv4 socket $TCP_HOST:$TCP_PORT" "$LOG_FILE"
grep -q "Accepted IPv4 connection" "$LOG_FILE"
grep -q "Activating service" "$LOG_FILE"

echo "IPv4 socket activation smoke test passed"
