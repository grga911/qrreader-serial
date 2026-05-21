#!/bin/sh
# Pack pre-built PyInstaller binary (dist/qrreader) into a .deb.
# Build the executable first: ./scripts/build-pyinstaller.sh
set -e
cd "$(dirname "$0")/.."

if ! command -v dpkg-buildpackage >/dev/null 2>&1; then
    echo "Install: sudo apt install debhelper devscripts file"
    exit 1
fi

if [ ! -f dist/qrreader ]; then
    echo "Missing dist/qrreader"
    echo "Run first: ./scripts/build-pyinstaller.sh"
    exit 1
fi

chmod +x dist/qrreader

VERSION="$(tr -d '\r\n' < VERSION)"
echo "Packing qrreader_${VERSION}-1 (binary) ..."

if ! grep -q "(${VERSION}-1)" debian/changelog; then
    echo "Warning: debian/changelog may not match VERSION (${VERSION})"
fi

chmod +x debian/rules 2>/dev/null || true

dpkg-buildpackage -us -uc -b

echo ""
echo "Done:"
echo "  ../qrreader_${VERSION}-1_amd64.deb"
echo ""
echo "Install:"
echo "  sudo dpkg -i ../qrreader_${VERSION}-1_amd64.deb"
echo "  systemctl --user enable --now qrreader.service"
