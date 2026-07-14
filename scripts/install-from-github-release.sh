#!/bin/sh
# Install or upgrade qrreader .deb from GitHub Releases.
#
# Usage:
#   GITHUB_REPO=owner/repo ./scripts/install-from-github-release.sh
#   GITHUB_REPO=owner/repo ./scripts/install-from-github-release.sh v1.0.0
#
# Installs curl/jq via apt when missing.
set -e
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)

# shellcheck source=./ensure-apt-packages.sh
. "$SCRIPT_DIR/ensure-apt-packages.sh"

REPO="${GITHUB_REPO:-}"
TAG="${1:-latest}"

if [ -z "$REPO" ]; then
    echo "Set GITHUB_REPO=owner/repo (your GitHub repository)."
    echo "Example: GITHUB_REPO=myorg/QR-READER-U22 $0"
    exit 1
fi

ensure_apt_packages curl jq ca-certificates

if [ "$TAG" = "latest" ]; then
    API="https://api.github.com/repos/${REPO}/releases/latest"
else
    case "$TAG" in v*) ;; *) TAG="v${TAG}" ;; esac
    API="https://api.github.com/repos/${REPO}/releases/tags/${TAG}"
fi

echo "==> Fetching release from ${REPO} (${TAG})"
JSON="$(curl -fsSL "$API")"
# Tag-based name: qrreader-v4.0.1_amd64.deb (preferred), else legacy *_amd64.deb
if [ "$TAG" != "latest" ]; then
    case "$TAG" in v*) ;; *) TAG="v${TAG}" ;; esac
    DEB_URL="$(echo "$JSON" | jq -r --arg t "$TAG" '.assets[] | select(.name == "qrreader-\($t)_amd64.deb") | .browser_download_url' | head -n1)"
fi
if [ -z "$DEB_URL" ] || [ "$DEB_URL" = "null" ]; then
    DEB_URL="$(echo "$JSON" | jq -r '.assets[] | select(.name | test("^qrreader-v[0-9].*_amd64\\.deb$")) | .browser_download_url' | head -n1)"
fi
if [ -z "$DEB_URL" ] || [ "$DEB_URL" = "null" ]; then
    DEB_URL="$(echo "$JSON" | jq -r '.assets[] | select(.name | endswith("_amd64.deb")) | .browser_download_url' | head -n1)"
fi

if [ -z "$DEB_URL" ] || [ "$DEB_URL" = "null" ]; then
    echo "No *_amd64.deb asset found on this release."
    echo "Assets:"
    echo "$JSON" | jq -r '.assets[].name'
    exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
DEB_FILE="$TMP/qrreader.deb"

echo "==> Downloading $(basename "$DEB_URL")"
curl -fsSL -o "$DEB_FILE" "$DEB_URL"

echo "==> Installing (sudo)"
sudo dpkg -i "$DEB_FILE" || sudo apt-get install -f -y

echo "==> Enable user service (run as desktop user, not root)"
echo "    systemctl --user daemon-reload"
echo "    systemctl --user enable --now qrreader.service"
