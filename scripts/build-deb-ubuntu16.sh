#!/bin/sh
# Pack pre-built PyInstaller binary (dist/qrreader) into a .deb on Ubuntu 16.04.
set -e
cd "$(dirname "$0")/.."

if [ ! -f dist/qrreader ]; then
    echo "Missing dist/qrreader — run: ./scripts/build-pyinstaller-ubuntu16.sh"
    exit 1
fi

missing=""
for f in debian/control debian/rules debian/install debian/changelog; do
    if [ ! -f "$f" ]; then
        missing="$missing $f"
    fi
done
if [ -n "$missing" ]; then
    echo "Missing debian packaging files:$missing"
    exit 1
fi

install_deb_tools() {
    if command -v dpkg-buildpackage >/dev/null 2>&1; then
        return
    fi
    echo "==> Installing deb packaging tools"
    export DEBIAN_FRONTEND=noninteractive
    if [ "$(id -u)" -eq 0 ]; then
        apt-get update
        apt-get install -y debhelper devscripts dpkg-dev file build-essential
    else
        sudo apt-get update
        sudo apt-get install -y debhelper devscripts dpkg-dev file build-essential
    fi
}

restore_debian() {
    if [ -f debian/control.bak.ubuntu16 ]; then
        mv -f debian/control.bak.ubuntu16 debian/control
    fi
    if [ -f debian/rules.bak.ubuntu16 ]; then
        mv -f debian/rules.bak.ubuntu16 debian/rules
    fi
    rm -f debian/compat
}

patch_debian_for_xenial() {
    cp debian/control debian/control.bak.ubuntu16
    cp debian/rules debian/rules.bak.ubuntu16
    sed 's/debhelper-compat (= 12)/debhelper (>= 9)/' debian/control.bak.ubuntu16 > debian/control
    sed 's/dh $@ --buildsystem=none/dh $@/' debian/rules.bak.ubuntu16 > debian/rules
    echo 9 > debian/compat
}

trap restore_debian EXIT INT TERM

install_deb_tools
patch_debian_for_xenial

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

echo "Packing qrreader_${DEB_VERSION} (Ubuntu 16.04 / debhelper 9) ..."
dpkg-buildpackage -us -uc -b

echo ""
echo "Done:"
ls -la ../*_${DEB_VERSION}_*.deb 2>/dev/null || ls -la ../*.deb 2>/dev/null || true
