#!/bin/sh
# Pack pre-built C++ binary (dist/qrreader) into a qrreader-cpp .deb.
set -e
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
cd "$SCRIPT_DIR/.."

# shellcheck source=./ensure-apt-packages.sh
. "$SCRIPT_DIR/ensure-apt-packages.sh"

ensure_apt_packages \
    debhelper debhelper-compat devscripts dpkg-dev file \
    build-essential fakeroot

if [ ! -f dist/qrreader ]; then
    echo "Missing dist/qrreader — run: ./scripts/build-cpp.sh"
    exit 1
fi

missing=""
for f in debian_cpp/control debian_cpp/rules debian_cpp/install debian_cpp/changelog; do
    if [ ! -f "$f" ]; then
        missing="$missing $f"
    fi
done
if [ -n "$missing" ]; then
    echo "Missing debian packaging files:$missing"
    echo "Ensure debian_cpp/ is committed."
    exit 1
fi

chmod 755 dist/qrreader

if [ -n "${GIT_TAG:-}" ]; then
    VERSION="${GIT_TAG#v}"
elif [ -n "${RELEASE_TAG:-}" ]; then
    VERSION="${RELEASE_TAG#v}"
else
    VERSION="$(tr -d '\r\n' < VERSION)"
fi
DEB_REV="${DEB_REV:-1}"
DEB_VERSION="${VERSION}-${DEB_REV}"

if ! dpkg-parsechangelog -l debian_cpp/changelog >/dev/null 2>&1; then
    echo "Error: debian_cpp/changelog is invalid"
    exit 1
fi

if ! dpkg-parsechangelog -l debian_cpp/changelog | grep -Fq "Version: ${DEB_VERSION}"; then
    echo "Warning: debian_cpp/changelog version does not match ${DEB_VERSION}"
fi

echo "Preparing debian/ from debian_cpp/..."
rm -rf debian
cp -a debian_cpp debian
chmod +x debian/rules

echo "Packing qrreader-cpp_${DEB_VERSION} ..."
dpkg-buildpackage -us -uc -b

echo ""
echo "Done:"
ls -la ../*_${DEB_VERSION}_*.deb 2>/dev/null || ls -la ../*.deb 2>/dev/null || true
