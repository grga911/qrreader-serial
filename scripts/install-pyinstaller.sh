#!/bin/sh
# Install PyInstaller binary and enable user systemd service.
set -e
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
cd "$SCRIPT_DIR/.."

# shellcheck source=./ensure-apt-packages.sh
. "$SCRIPT_DIR/ensure-apt-packages.sh"

if [ "$(id -u)" -eq 0 ]; then
    echo "Run without sudo; only 'sudo' is used to copy the binary to /usr/local/bin."
    exit 1
fi

if [ ! -f dist/qrreader ]; then
    echo "Missing dist/qrreader — run: ./scripts/build-pyinstaller.sh"
    exit 1
fi

# Runtime helpers used for clipboard paste on X11.
ensure_apt_packages xclip xdotool

echo "==> Installing binary to /usr/local/bin/qrreader (sudo)"
sudo install -m 755 dist/qrreader /usr/local/bin/qrreader

USER_UNIT_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
mkdir -p "$USER_UNIT_DIR"
cp qrreader.service "$USER_UNIT_DIR/qrreader.service"

echo "==> Enabling user service"
systemctl --user daemon-reload
systemctl --user enable qrreader.service
systemctl --user restart qrreader.service

echo ""
systemctl --user status qrreader.service --no-pager || true
echo ""
echo "Logs: journalctl --user -u qrreader.service -f"
echo "Port config (optional): create /etc/qrreader/port or ~/.config/qrreader/port"
