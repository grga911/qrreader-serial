#!/bin/sh
# Point installed qrreader at GitHub VERSION file for --check-update.
# Usage: GITHUB_REPO=owner/repo ./scripts/configure-update-url.sh
set -e

REPO="${GITHUB_REPO:-}"
BRANCH="${GITHUB_BRANCH:-main}"

if [ -z "$REPO" ]; then
    echo "Set GITHUB_REPO=owner/repo"
    exit 1
fi

URL="https://raw.githubusercontent.com/${REPO}/${BRANCH}/VERSION"
CONF="/etc/qrreader/update.url"

echo "$URL" | sudo tee "$CONF" >/dev/null
echo "Wrote $CONF"
echo "Check: qrreader --check-update  (if binary supports it)"
