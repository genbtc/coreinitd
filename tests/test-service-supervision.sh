#!/bin/sh
set -eu

BIN=${COREINITD_BIN:-./build/coreinitd}
TMPDIR=$(mktemp -d "${TMPDIR:-/tmp}/coreinitd-supervision.XXXXXX")
LOG="$TMPDIR/coreinitd.log"
RUNS="$TMPDIR/runs.log"
trap 'rm -rf "$TMPDIR"' EXIT INT TERM

cat > "$TMPDIR/restart-on-failure.service" <<EOF_SERVICE
[Unit]
Description=Restart-on-failure supervision target
StartLimitBurst=2

[Service]
ExecStart=/bin/sh -c 'printf x >> "$RUNS"; exit 1'
Restart=on-failure
RestartSec=0
EOF_SERVICE

"$BIN" --unit-dir "$TMPDIR" --no-sockets --no-timers --run-for-sec 2 >"$LOG" 2>&1

runs=0
if [ -f "$RUNS" ]; then
    runs=$(wc -c < "$RUNS" | tr -d ' ')
fi

if [ "$runs" -ne 2 ]; then
    echo "expected service to run exactly twice before StartLimitBurst stopped restarts, got $runs" >&2
    cat "$LOG" >&2
    exit 1
fi

if ! grep -q "Scheduled restart for .*restart-on-failure.service" "$LOG"; then
    echo "expected restart scheduling log" >&2
    cat "$LOG" >&2
    exit 1
fi

if ! grep -q "Start limit hit for .*restart-on-failure.service" "$LOG"; then
    echo "expected start-limit log" >&2
    cat "$LOG" >&2
    exit 1
fi

echo "Restart=on-failure supervision restarted once and honored StartLimitBurst"
