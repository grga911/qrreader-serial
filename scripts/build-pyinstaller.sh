#!/bin/sh
# Build standalone qrreader binary with PyInstaller (Linux).
set -e
cd "$(dirname "$0")/.."

VENV_DIR="${VENV_DIR:-.venv-build}"
PYTHON="${PYTHON:-python3}"
REQUIREMENTS_BUILD="${REQUIREMENTS_BUILD:-requirements-build.txt}"

echo "==> Creating venv: $VENV_DIR (interpreter: $PYTHON)"
"$PYTHON" -m venv "$VENV_DIR"
# shellcheck disable=SC1091
. "$VENV_DIR/bin/activate"

if python -c 'import sys; raise SystemExit(0 if sys.version_info < (3, 7) else 1)'; then
    python -m pip install --upgrade 'pip<22' wheel setuptools
else
    python -m pip install --upgrade pip wheel
fi

echo "==> Installing requirements from $REQUIREMENTS_BUILD"
pip install -r "$REQUIREMENTS_BUILD"

echo "==> Removing non-required packages (non-requirements.txt)"
while IFS= read -r pkg || [ -n "$pkg" ]; do
    case "$pkg" in
        ''|\#*) continue ;;
    esac
    pip uninstall -y "$pkg" 2>/dev/null || true
done < non-requirements.txt

echo "==> PyInstaller build (qrreader.spec)"
pyinstaller --clean --noconfirm qrreader.spec

if [ ! -f dist/qrreader ]; then
    echo "Build failed: dist/qrreader not found"
    exit 1
fi

chmod +x dist/qrreader
echo ""
echo "Built: $(pwd)/dist/qrreader"
echo "Install: sudo scripts/install-pyinstaller.sh"
