#!/bin/sh
# Full release: PyInstaller executable + .deb package (same machine arch as target).
set -e
cd "$(dirname "$0")/.."

./scripts/build-pyinstaller.sh
./scripts/build-deb.sh
