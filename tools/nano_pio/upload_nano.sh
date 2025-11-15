#!/usr/bin/env bash
# Simple wrapper to call the Python upload helper (upload_nano.py).
# Keeps a compatible CLI but delegates the real work to the Python script.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY_HELPER="$SCRIPT_DIR/upload_nano.py"

# Validate Python helper exists
if [ ! -f "$PY_HELPER" ]; then
    echo "Error: Python helper not found: $PY_HELPER" >&2
    exit 1
fi

if [ ! -x "$PY_HELPER" ]; then
    echo "Error: Python helper not executable: $PY_HELPER" >&2
    exit 1
fi

# Handle port argument
if [ "$#" -ge 1 ] && [ "$1" != "" ]; then
    UPLOAD_PORT="$1"
    shift
else
    # Auto-detect port
    UPLOAD_PORT=""
    for p in "/dev/ttyUSB0" "/dev/ttyUSB1" "/dev/ttyACM0" "/dev/ttyACM1"; do
        if [ -e "$p" ]; then
            UPLOAD_PORT="$p"
            break
        fi
    done
fi

# Validate port exists or use default
: "${UPLOAD_PORT:="/dev/ttyUSB0"}"

if [ ! -e "$UPLOAD_PORT" ]; then
    echo "Warning: Port $UPLOAD_PORT does not exist" >&2
fi

# Use exec to replace shell with Python process
exec python3 "$PY_HELPER" --port "$UPLOAD_PORT" "$@"
