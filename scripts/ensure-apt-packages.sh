#!/bin/sh
# Ensure Debian/Ubuntu packages are installed (apt-get).
#
# Usage (standalone):
#   ./scripts/ensure-apt-packages.sh pkg1 pkg2 ...
#
# Usage (sourced from another script):
#   # shellcheck source=./ensure-apt-packages.sh
#   . "$(dirname "$0")/ensure-apt-packages.sh"
#   ensure_apt_packages pkg1 pkg2 ...
#
# Skips work when every package is already installed.
# Uses sudo when not running as root.
ensure_apt_packages() {
    if [ "$#" -eq 0 ]; then
        echo "ensure_apt_packages: no packages specified" >&2
        return 1
    fi

    if ! command -v dpkg-query >/dev/null 2>&1; then
        echo "ensure_apt_packages: dpkg-query not found (Debian/Ubuntu required)" >&2
        return 1
    fi

    missing=""
    for pkg in "$@"; do
        if ! dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q 'install ok installed'; then
            missing="$missing $pkg"
        fi
    done
    missing=$(echo "$missing" | sed 's/^ *//')

    if [ -z "$missing" ]; then
        return 0
    fi

    echo "==> Installing apt packages: $missing"

    # shellcheck disable=SC2086
    _ensure_apt_run update -qq
    # shellcheck disable=SC2086
    _ensure_apt_run install -y $missing
}

_ensure_apt_run() {
    export DEBIAN_FRONTEND=noninteractive
    if [ "$(id -u)" -eq 0 ]; then
        apt-get "$@"
    else
        if ! command -v sudo >/dev/null 2>&1; then
            echo "ensure_apt_packages: sudo required to install packages" >&2
            return 1
        fi
        sudo DEBIAN_FRONTEND=noninteractive apt-get "$@"
    fi
}

# Allow running as a standalone script: ./ensure-apt-packages.sh pkg...
case "${0##*/}" in
ensure-apt-packages.sh)
    set -e
    ensure_apt_packages "$@"
    ;;
esac
