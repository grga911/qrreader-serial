#!/bin/sh
# Build standalone qrreader binary from the Linux C++ sources (cmake).
#
# Produces: dist/qrreader
#
# Optional cmake flags (pass as env):
#   QRREADER_STATIC_LIBSTDCPP=ON   # embed libstdc++ / libgcc (portable glibc builds)
#   QRREADER_FULLY_STATIC=ON       # full -static (prefer musl toolchain)
set -e
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
cd "$SCRIPT_DIR/.."

# shellcheck source=./ensure-apt-packages.sh
. "$SCRIPT_DIR/ensure-apt-packages.sh"

ensure_apt_packages build-essential cmake binutils

BUILD_DIR="${BUILD_DIR:-build-linux}"
CXX="${CXX:-g++}"
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=${CXX}"

if [ -n "${QRREADER_STATIC_LIBSTDCPP:-}" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DQRREADER_STATIC_LIBSTDCPP=${QRREADER_STATIC_LIBSTDCPP}"
fi
if [ -n "${QRREADER_FULLY_STATIC:-}" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DQRREADER_FULLY_STATIC=${QRREADER_FULLY_STATIC}"
fi

echo "==> Configuring C++ build ($BUILD_DIR, CXX=$CXX)"
rm -rf "$BUILD_DIR" dist
mkdir -p dist

# shellcheck disable=SC2086
cmake -S src -B "$BUILD_DIR" $CMAKE_ARGS

echo "==> Building qrreader_linux"
cmake --build "$BUILD_DIR" --config Release -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"

if [ ! -f "$BUILD_DIR/qrreader_linux" ]; then
    echo "Build failed: $BUILD_DIR/qrreader_linux not found"
    exit 1
fi

if command -v strip >/dev/null 2>&1; then
    strip --strip-unneeded "$BUILD_DIR/qrreader_linux" 2>/dev/null || true
fi

cp "$BUILD_DIR/qrreader_linux" dist/qrreader
chmod 755 dist/qrreader

echo ""
echo "Built: $(pwd)/dist/qrreader ($(du -h dist/qrreader | cut -f1))"
file dist/qrreader
echo "Pack .deb: ./scripts/build-deb-cpp.sh"
echo "Full release: ./scripts/build-release-cpp.sh"
