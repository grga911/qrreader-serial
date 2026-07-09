#!/bin/sh
# Build C++ Linux binary and pack it into a .deb.
set -e
cd "$(dirname "$0")/.."

if ! command -v dpkg-buildpackage >/dev/null 2>&1; then
    echo "Install: sudo apt install debhelper devscripts file"
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "Install: sudo apt install cmake build-essential"
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

echo "Building C++ binary..."
rm -rf build-linux dist
mkdir -p dist

cmake -S src -B build-linux -DCMAKE_BUILD_TYPE=Release

cmake --build build-linux --config Release -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
strip build-linux/qrreader_linux -o build-linux/qrreader_linux.stripped
mv build-linux/qrreader_linux.stripped build-linux/qrreader_linux

if [ ! -f build-linux/qrreader_linux ]; then
    echo "Error: build output missing: build-linux/qrreader_linux"
    exit 1
fi

cp build-linux/qrreader_linux dist/qrreader
chmod 755 dist/qrreader

echo "Built binary:"
file dist/qrreader

echo "Packing qrreader-cpp_${DEB_VERSION} ..."
dpkg-buildpackage -us -uc -b

echo ""
echo "Done:"
ls -la ../*_${DEB_VERSION}_*.deb 2>/dev/null || ls -la ../*.deb 2>/dev/null || true