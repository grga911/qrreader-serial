#!/bin/sh
# Pack pre-built PyInstaller binary (dist/qrreader) into a .deb.
set -e
cd "$(dirname "$0")/.."

if ! command -v dpkg-buildpackage >/dev/null 2>&1; then
    echo "Install: sudo apt install debhelper devscripts file"
    exit 1
fi

if [ ! -f dist/qrreader ]; then
    echo "Missing dist/qrreader — run: ./scripts/build-pyinstaller.sh"
    exit 1
fi

chmod +x dist/qrreader

VERSION="$(tr -d '\r\n' < VERSION)"
DEB_REV="${DEB_REV:-1}"
DEB_VERSION="${VERSION}-${DEB_REV}"

mkdir -p debian

# Ensure debian/changelog exists and matches VERSION
if [ ! -f debian/changelog ]; then
    echo "Creating debian/changelog for ${DEB_VERSION}"
    cat > debian/changelog <<EOF
qrreader (${DEB_VERSION}) unstable; urgency=medium

  * Release ${VERSION} (PyInstaller binary)

 -- QR Reader <qrreader@localhost>  $(date -R)

EOF
elif ! grep -q "(${DEB_VERSION})" debian/changelog 2>/dev/null; then
    echo "Warning: debian/changelog has no entry (${DEB_VERSION})"
    echo "  Edit debian/changelog or delete it to auto-regenerate."
fi

chmod +x debian/rules 2>/dev/null || true

echo "Packing qrreader_${DEB_VERSION} ..."
dpkg-buildpackage -us -uc -b

echo ""
echo "Done. Look for:"
echo "  ../qrreader_${DEB_VERSION}_amd64.deb"
ls -la ../*.deb 2>/dev/null || true
