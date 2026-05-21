#!/bin/sh
# Build apt repository metadata (Packages, Release) from .deb files in a directory.
#
# Usage:
#   ./scripts/setup-apt-repo.sh ./apt-repo
#   ./scripts/setup-apt-repo.sh ./apt-repo ../*.deb
set -e

REPO_DIR="${1:-./apt-repo}"
shift || true

if ! command -v apt-ftparchive >/dev/null 2>&1; then
    echo "Install: sudo apt install apt-utils"
    exit 1
fi

mkdir -p "$REPO_DIR"

if [ "$#" -gt 0 ]; then
    cp -f "$@" "$REPO_DIR/"
elif [ -n "${DEB_GLOB:-}" ]; then
    # shellcheck disable=SC2086
    cp -f $DEB_GLOB "$REPO_DIR/" 2>/dev/null || true
fi

if ! ls "$REPO_DIR"/*.deb >/dev/null 2>&1; then
    echo "No .deb files in $REPO_DIR"
    echo "Usage: $0 <repo-dir> [path/to/*.deb ...]"
    exit 1
fi

cd "$REPO_DIR"
apt-ftparchive packages . > Packages
gzip -9c Packages > Packages.gz
apt-ftparchive release . > Release

echo "APT repo ready: $(pwd)"
ls -la
