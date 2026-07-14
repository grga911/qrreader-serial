#!/bin/sh
# Full C++ release: cmake executable + qrreader-cpp .deb (same machine arch as target).
set -e
cd "$(dirname "$0")/.."

./scripts/build-cpp.sh
./scripts/build-deb-cpp.sh
