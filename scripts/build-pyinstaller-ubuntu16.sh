#!/bin/sh
# Install Python 3.6 on Ubuntu 16.04 and build qrreader with PyInstaller.
#
# Ubuntu 16.04 ships Python 3.5; this script compiles Python 3.6.15 into /usr/local
# (skipped when python3.6 is already present), then runs the PyInstaller build.
#
# Usage (on Ubuntu 16.04 build machine):
#   chmod +x scripts/build-pyinstaller-ubuntu16.sh
#   ./scripts/build-pyinstaller-ubuntu16.sh
#
# Optional env:
#   SKIP_PYTHON_BUILD=1   — require existing python3.6 in PATH (no compile)
#   PYTHON36_PREFIX=/usr/local
#   VENV_DIR=.venv-build-ubuntu16
#
set -e
cd "$(dirname "$0")/.."
SCRIPT="$0"

PYTHON_RELEASE=3.6.15
PYTHON36_PREFIX="${PYTHON36_PREFIX:-/usr/local}"
PYTHON36="${PYTHON36:-$PYTHON36_PREFIX/bin/python3.6}"
VENV_DIR="${VENV_DIR:-.venv-build-ubuntu16}"
PIP_VERSION=21.3.1
BUILD_DIR="${TMPDIR:-/tmp}/qrreader-python${PYTHON_RELEASE}-build"

ubuntu_major() {
    if [ -r /etc/os-release ]; then
        # shellcheck disable=SC1091
        . /etc/os-release
        echo "${VERSION_ID%%.*}"
        return
    fi
    if command -v lsb_release >/dev/null 2>&1; then
        lsb_release -sr | cut -d. -f1
        return
    fi
    echo "unknown"
}

python36_ready() {
    command -v "$PYTHON36" >/dev/null 2>&1 \
        && "$PYTHON36" -c 'import sys; raise SystemExit(0 if sys.version_info[:2] == (3, 6) else 1)'
}

require_root() {
    if [ "$(id -u)" -ne 0 ]; then
        exec sudo -E env \
            SKIP_PYTHON_BUILD="${SKIP_PYTHON_BUILD:-}" \
            PYTHON36_PREFIX="$PYTHON36_PREFIX" \
            PYTHON36="$PYTHON36" \
            VENV_DIR="$VENV_DIR" \
            "$SCRIPT" "$@"
    fi
}

require_user() {
    if [ "$(id -u)" -eq 0 ]; then
        echo "Run this script as a normal user (not root)."
        exit 1
    fi
}

install_apt_packages() {
    echo "==> Installing apt build dependencies"
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y \
        build-essential \
        curl \
        wget \
        ca-certificates \
        libssl-dev \
        zlib1g-dev \
        libbz2-dev \
        libreadline-dev \
        libsqlite3-dev \
        libncurses5-dev \
        libncursesw5-dev \
        xz-utils \
        libffi-dev \
        liblzma-dev \
        tk-dev \
        binutils \
        patchelf \
        file
}

install_python36_from_source() {
    if [ "${SKIP_PYTHON_BUILD:-0}" = "1" ]; then
        echo "SKIP_PYTHON_BUILD=1 but $PYTHON36 is missing or not 3.6"
        exit 1
    fi

    echo "==> Building Python ${PYTHON_RELEASE} from source (first run may take ~10–20 min)"
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    src="Python-${PYTHON_RELEASE}.tgz"
    wget -q "https://www.python.org/ftp/python/${PYTHON_RELEASE}/${src}"
    tar -xzf "$src"
    cd "Python-${PYTHON_RELEASE}"

    ./configure \
        --prefix="$PYTHON36_PREFIX" \
        --enable-shared \
        LDFLAGS="-Wl,-rpath,$PYTHON36_PREFIX/lib"

    make -j"$(nproc 2>/dev/null || echo 2)"
    make altinstall

    cd /
    rm -rf "$BUILD_DIR"

    if ! python36_ready; then
        echo "Python 3.6 install failed: $PYTHON36 not found"
        exit 1
    fi

    ldconfig "$PYTHON36_PREFIX/lib" 2>/dev/null || true
    echo "==> Installed: $("$PYTHON36" --version)"
}

install_pip_for_python36() {
    if "$PYTHON36" -m pip --version >/dev/null 2>&1; then
        echo "==> pip already available for $PYTHON36"
        return
    fi
    echo "==> Bootstrapping pip for Python 3.6"
    curl -fsSL "https://bootstrap.pypa.io/pip/${PYTHON_RELEASE%.*}/get-pip.py" -o /tmp/get-pip.py
    "$PYTHON36" /tmp/get-pip.py "pip==${PIP_VERSION}" setuptools wheel
    rm -f /tmp/get-pip.py
}

cmd_install_python() {
    require_root
    install_apt_packages
    if ! python36_ready; then
        install_python36_from_source
    else
        echo "==> Python 3.6 already present: $("$PYTHON36" --version)"
    fi
    install_pip_for_python36
}

build_pyinstaller_binary() {
    require_user

    echo "==> Creating venv: $VENV_DIR"
    rm -rf "$VENV_DIR"
    "$PYTHON36" -m venv "$VENV_DIR"
    # shellcheck disable=SC1091
    . "$VENV_DIR/bin/activate"

    python -m pip install --upgrade "pip==${PIP_VERSION}" wheel

    echo "==> Installing Python 3.6 build requirements"
    pip install -r requirements-build-ubuntu16.txt

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
    echo "Package: ./scripts/build-deb-ubuntu16.sh"
    echo "Full release: ./scripts/build-release-ubuntu16.sh"
    "$PYTHON36" --version
    pyinstaller --version
}

case "${1:-}" in
    --install-python)
        cmd_install_python
        ;;
    --build-only)
        build_pyinstaller_binary
        ;;
    "")
        require_user
        major="$(ubuntu_major)"
        if [ "$major" != "16" ] && [ "$major" != "unknown" ]; then
            echo "Warning: expected Ubuntu 16.04, detected major version $major"
        fi
        if ! python36_ready || ! "$PYTHON36" -m pip --version >/dev/null 2>&1; then
            sudo -E env \
                SKIP_PYTHON_BUILD="${SKIP_PYTHON_BUILD:-}" \
                PYTHON36_PREFIX="$PYTHON36_PREFIX" \
                PYTHON36="$PYTHON36" \
                VENV_DIR="$VENV_DIR" \
                "$SCRIPT" --install-python
        fi
        build_pyinstaller_binary
        ;;
    *)
        echo "Usage: $SCRIPT [--install-python | --build-only]"
        exit 1
        ;;
esac
