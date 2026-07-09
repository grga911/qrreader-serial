#!/bin/sh
# Full release on Ubuntu 16.04: Python 3.6 + PyInstaller binary + .deb package.
set -e
cd "$(dirname "$0")/.."

sh ./scripts/build-pyinstaller-ubuntu16.sh
sh ./scripts/build-deb-ubuntu16.sh
