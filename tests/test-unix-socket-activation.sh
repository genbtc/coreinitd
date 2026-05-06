#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

SOCKET_PATH=/tmp/coreinitd-example.sock
LOG_FILE=$(mktemp)
PID=
cleanup() {
    if [ -n "${PID:-}" ] && kill -0 "$PID" 2>/dev/null; then
        kill -TERM "$PID" 2>/dev/null || true
        wait "$PID" 2>/dev/null || true
    fi
    rm -f "$SOCKET_PATH" "$LOG_FILE"
}
trap cleanup EXIT INT TERM

if [ ! -x ./build/coreinitd ]; then
    echo "./build/coreinitd is missing; build with: meson setup build && meson compile -C build" >&2
    exit 77
fi

./build/coreinitd >"$LOG_FILE" 2>&1 &
PID=$!

python3 - <<'PY'
import pathlib
import socket
import sys
import time

path = pathlib.Path('/tmp/coreinitd-example.sock')
for _ in range(50):
    if path.exists():
        break
    time.sleep(0.1)
else:
    print('UNIX socket was not created', file=sys.stderr)
    sys.exit(1)

client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
client.connect(str(path))
client.close()
PY

sleep 1
kill -TERM "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true
PID=

grep -q "Listening on UNIX socket $SOCKET_PATH" "$LOG_FILE"
grep -q "Accepted UNIX connection" "$LOG_FILE"
grep -q "Activating service" "$LOG_FILE"

echo "UNIX socket activation smoke test passed"
