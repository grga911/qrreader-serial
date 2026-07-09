#!/bin/sh
# Mock xdotool: records argv to MOCK_XDOTOOL_LOG and exits with MOCK_XDOTOOL_EXIT.
set -eu

if [ -z "${MOCK_XDOTOOL_LOG:-}" ]; then
    echo "mock-xdotool: MOCK_XDOTOOL_LOG is not set" >&2
    exit 1
fi

printf '%s\n' "$@" > "$MOCK_XDOTOOL_LOG"
exit "${MOCK_XDOTOOL_EXIT:-0}"
