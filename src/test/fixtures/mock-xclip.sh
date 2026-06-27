#!/bin/sh
# Mock xclip: writes stdin to MOCK_XCLIP_OUT_FILE, or prints MOCK_XCLIP_IN when -o is passed.
set -eu

for arg in "$@"; do
    if [ "$arg" = "-o" ]; then
        if [ -n "${MOCK_XCLIP_IN:-}" ]; then
            printf '%s' "$MOCK_XCLIP_IN"
        elif [ -n "${MOCK_XCLIP_IN_FILE:-}" ] && [ -f "$MOCK_XCLIP_IN_FILE" ]; then
            cat "$MOCK_XCLIP_IN_FILE"
        fi
        exit "${MOCK_XCLIP_READ_EXIT:-0}"
    fi
done

if [ -z "${MOCK_XCLIP_OUT_FILE:-}" ]; then
    echo "mock-xclip: MOCK_XCLIP_OUT_FILE is not set" >&2
    exit 1
fi

cat > "$MOCK_XCLIP_OUT_FILE"
exit "${MOCK_XCLIP_WRITE_EXIT:-0}"
