#!/bin/sh
# Install apt packages for release builds inside the workflow container.
# Writes build-env.sh for the PyInstaller step (PYTHON / REQUIREMENTS_BUILD).
set -eu

BUILD_IMAGE="${1:-ubuntu:20.04}"
ENV_FILE="${2:-build-env.sh}"

apt-get update

case "$BUILD_IMAGE" in
    ubuntu:16.04)
        DEBIAN_FRONTEND=noninteractive apt-get install -y \
            python3.6 python3.6-venv python3.6-dev \
            debhelper devscripts file binutils ca-certificates git \
            build-essential libx11-dev libxtst-dev libpng-dev \
            scrot python3-xlib
        cat > "$ENV_FILE" <<'EOF'
PYTHON=python3.6
REQUIREMENTS_BUILD=requirements-build-py36.txt
EOF
        echo "Build profile: Ubuntu 16.04 / Python 3.6"
        ;;
    *)
        DEBIAN_FRONTEND=noninteractive apt-get install -y \
            python3 python3-venv python3-pip \
            debhelper debhelper-compat devscripts file binutils ca-certificates git
        cat > "$ENV_FILE" <<'EOF'
PYTHON=python3
REQUIREMENTS_BUILD=requirements-build.txt
EOF
        echo "Build profile: default Python 3"
        ;;
esac

echo "Wrote $ENV_FILE:"
cat "$ENV_FILE"

PYTHON="$(grep '^PYTHON=' "$ENV_FILE" | cut -d= -f2-)"
"$PYTHON" --version
