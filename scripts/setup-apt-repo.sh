#!/bin/sh
# Create a simple local apt repository from built .deb files (for LAN updates).
set -e

REPO_DIR="${1:-./apt-repo}"
DEB_GLOB="${2:-../*.deb}"

mkdir -p "$REPO_DIR"
cp -f $DEB_GLOB "$REPO_DIR/" 2>/dev/null || {
    echo "No .deb files found. Run scripts/build-deb.sh first."
    exit 1
}

cd "$REPO_DIR"
apt-ftparchive packages . > Packages
gzip -9c Packages > Packages.gz
apt-ftparchive release . > Release

echo ""
echo "Repository ready: $(pwd)"
echo ""
echo "On client machines, add (replace HOST and path):"
echo '  echo "deb [trusted=yes] file:/path/to/apt-repo ./" | sudo tee /etc/apt/sources.list.d/qrreader.list'
echo "Or HTTP:"
echo '  echo "deb [trusted=yes] http://HOST/qrreader ./" | sudo tee /etc/apt/sources.list.d/qrreader.list'
echo "  sudo apt update"
echo "  sudo apt install qrreader"
echo "  sudo apt upgrade qrreader   # future updates"
