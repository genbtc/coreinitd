#!/bin/sh
set -eu

BIN=${COREINITD_BIN:-./build/coreinitd}
TMPDIR=$(mktemp -d "${TMPDIR:-/tmp}/coreinitd-timer.XXXXXX")
LOG="$TMPDIR/coreinitd.log"
RUNS="$TMPDIR/runs.log"
trap 'rm -rf "$TMPDIR"' EXIT INT TERM

cat > "$TMPDIR/recurring.service" <<EOF_SERVICE
[Unit]
Description=Recurring timer target

[Service]
ExecStart=/bin/sh -c 'printf fired >> "$RUNS"'
EOF_SERVICE

cat > "$TMPDIR/recurring.timer" <<'EOF_TIMER'
[Unit]
Description=Recurring timer test

[Timer]
OnBootSec=1s
OnUnitActiveSec=1s
Unit=recurring.service
EOF_TIMER

"$BIN" --unit-dir "$TMPDIR" --no-services --no-sockets --timer-fires 3 >"$LOG" 2>&1

fires=$(grep -c "Timer fired: .*recurring.timer" "$LOG" || true)
if [ "$fires" -lt 3 ]; then
    echo "expected at least 3 timer fires, got $fires" >&2
    cat "$LOG" >&2
    exit 1
fi

if ! grep -q "Rescheduled .*recurring.timer" "$LOG"; then
    echo "expected recurring timer reschedule log" >&2
    cat "$LOG" >&2
    exit 1
fi

echo "OnUnitActiveSec recurrence observed $fires timer fires"
