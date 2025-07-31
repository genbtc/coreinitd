#!/bin/bash
# start-unit.sh - Launches a service unit file (e.g., foo.service)

set -euo pipefail

UNIT_DIR="./etc/units"
UNIT_FILE="$UNIT_DIR/$1"

if [[ ! -f "$UNIT_FILE" ]]; then
    echo "Unit $1 not found in $UNIT_DIR"
    exit 1
fi

# Load unit values
ExecStart=""
NotifyAccess=""
Socket=""
Sandbox=""

while IFS='=' read -r key value; do
    case "$key" in
        ExecStart) ExecStart="$value" ;;
        NotifyAccess) NotifyAccess="$value" ;;
        Socket) Socket="$value" ;;
        Sandbox) Sandbox="$value" ;;
    esac
done < <(grep -vE '^\[.*\]' "$UNIT_FILE" | grep '=')

if [[ -z "$ExecStart" ]]; then
    echo "No ExecStart specified"
    exit 1
fi

echo "[+] Starting unit: $1"
echo "    Command: $ExecStart"
[[ -n "$Socket" ]] && echo "    Socket: $Socket"
[[ "$Sandbox"]()]()
