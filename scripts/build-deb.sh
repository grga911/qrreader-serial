#!/bin/sh
# Pack pre-built PyInstaller binary (dist/qrreader) into a .deb.
set -e
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
cd "$SCRIPT_DIR/.."

# shellcheck source=./ensure-apt-packages.sh
. "$SCRIPT_DIR/ensure-apt-packages.sh"

ensure_apt_packages debhelper debhelper-compat devscripts dpkg-dev file build-essential fakeroot

if [ ! -f dist/qrreader ]; then
    echo "Missing dist/qrreader — run: ./scripts/build-pyinstaller.sh"
    exit 1
fi

# Required debian packaging files (must be in git — see .gitignore)
missing=""
for f in debian/control debian/rules debian/install debian/changelog; do
    if [ ! -f "$f" ]; then
        missing="$missing $f"
    fi
done
if [ -n "$missing" ]; then
    echo "Missing debian packaging files:$missing"
    echo "Ensure debian/ is committed (it must NOT be listed in .gitignore)."
    exit 1
fi

chmod +x dist/qrreader debian/rules

if [ -n "${GIT_TAG:-}" ]; then
    VERSION="${GIT_TAG#v}"
elif [ -n "${RELEASE_TAG:-}" ]; then
    VERSION="${RELEASE_TAG#v}"
else
    VERSION="$(tr -d '\r\n' < VERSION)"
fi
DEB_REV="${DEB_REV:-1}"
DEB_VERSION="${VERSION}-${DEB_REV}"

if ! grep -q "(${DEB_VERSION})" debian/changelog; then
    echo "Warning: debian/changelog has no entry for (${DEB_VERSION})"
fi

echo "Packing qrreader_${DEB_VERSION} ..."
dpkg-buildpackage -us -uc -b

echo ""
echo "Done:"
ls -la ../*_${DEB_VERSION}_*.deb 2>/dev/null || ls -la ../*.deb 2>/dev/null || true
