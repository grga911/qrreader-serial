#!/bin/sh
# Build standalone qrreader binary with PyInstaller (Linux).
set -e
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
cd "$SCRIPT_DIR/.."

# shellcheck source=./ensure-apt-packages.sh
. "$SCRIPT_DIR/ensure-apt-packages.sh"

# python3-venv is required for `python3 -m venv`; binutils for strip/objdump.
ensure_apt_packages python3 python3-venv python3-pip binutils

VENV_DIR="${VENV_DIR:-.venv-build}"
PYTHON="${PYTHON:-python3}"

echo "==> Creating venv: $VENV_DIR"
"$PYTHON" -m venv "$VENV_DIR"
# shellcheck disable=SC1091
. "$VENV_DIR/bin/activate"

python -m pip install --upgrade pip wheel

echo "==> Installing requirements"
pip install -r requirements-build.txt

echo "==> Removing non-required packages (non-requirements.txt)"
while IFS= read -r pkg || [ -n "$pkg" ]; do
    case "$pkg" in
        ''|\#*) continue ;;
    esac
    pip uninstall -y "$pkg" 2>/dev/null || true
done < non-requirements.txt

echo "==> PyInstaller build (qrreader.spec)"
rm -rf build dist
pyinstaller --clean --noconfirm qrreader.spec

if [ ! -f dist/qrreader ]; then
    echo "Build failed: dist/qrreader not found"
    exit 1
fi

if command -v strip >/dev/null 2>&1; then
    strip --strip-unneeded dist/qrreader 2>/dev/null || true
fi

chmod +x dist/qrreader
echo ""
echo "Built: $(pwd)/dist/qrreader ($(du -h dist/qrreader | cut -f1))"
echo "Install: sudo scripts/install-pyinstaller.sh"
