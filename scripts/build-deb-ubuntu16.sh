#!/bin/sh
# Pack pre-built PyInstaller binary (dist/qrreader) into a .deb on Ubuntu 16.04.
set -e
cd "$(dirname "$0")/.."

if [ ! -f dist/qrreader ]; then
    echo "Missing dist/qrreader — run: ./scripts/build-pyinstaller-ubuntu16.sh"
    exit 1
fi

missing=""
for f in debian/control debian/rules debian/install debian/changelog; do
    if [ ! -f "$f" ]; then
        missing="$missing $f"
    fi
done
if [ -n "$missing" ]; then
    echo "Missing debian packaging files:$missing"
    exit 1
fi

apt_run() {
    export DEBIAN_FRONTEND=noninteractive
    if [ "$(id -u)" -eq 0 ]; then
        apt-get "$@"
    else
        sudo apt-get "$@"
    fi
}

install_deb_tools() {
    echo "==> Ensuring deb packaging tools (debhelper, devscripts, ...)"
    apt_run update
    apt_run install -y debhelper devscripts dpkg-dev file build-essential fakeroot
}

sync_changelog_from_version() {
    if [ -n "${GIT_TAG:-}" ]; then
        VERSION="${GIT_TAG#v}"
    elif [ -n "${RELEASE_TAG:-}" ]; then
        VERSION="${RELEASE_TAG#v}"
    else
        VERSION="$(tr -d '\r\n' < VERSION)"
    fi
    DEB_REV="${DEB_REV:-1}"
    DEB_VERSION="${VERSION}-${DEB_REV}"

    if grep -q "(${DEB_VERSION})" debian/changelog; then
        return
    fi

    echo "==> Updating debian/changelog to (${DEB_VERSION}) from VERSION"
    tmp_old=debian/changelog.sync.bak
    cp debian/changelog "$tmp_old"
    {
        printf 'qrreader (%s) unstable; urgency=medium\n\n' "$DEB_VERSION"
        printf '  * Release %s\n\n' "$VERSION"
        printf ' -- QR Reader Maintainer <qrreader@localhost>  %s\n\n' "$(date -R)"
        cat "$tmp_old"
    } > debian/changelog
    rm -f "$tmp_old"
}

restore_debian() {
    if [ -f debian/control.bak.ubuntu16 ]; then
        mv -f debian/control.bak.ubuntu16 debian/control
    fi
    if [ -f debian/rules.bak.ubuntu16 ]; then
        mv -f debian/rules.bak.ubuntu16 debian/rules
    fi
    rm -f debian/compat
}

write_rules_ubuntu16() {
    cat > debian/rules <<'EOF'
#!/usr/bin/make -f
export DH_VERBOSE = 1

# Ubuntu 16.04 / debhelper 9: explicit installs (do not rely on debian/install manifest).
%:
	dh $@

override_dh_auto_configure:

override_dh_autoreconf:

override_dh_auto_build:
	@test -f dist/qrreader || (echo "Missing dist/qrreader — run: ./scripts/build-pyinstaller.sh" && exit 1)
	@file dist/qrreader | grep -qE 'ELF .+ executable' || (echo "dist/qrreader is not a Linux ELF binary" && exit 1)
	chmod 755 dist/qrreader

override_dh_auto_install:

override_dh_auto_test:

override_dh_install:
	install -d debian/qrreader/usr/bin
	install -d debian/qrreader/usr/share/qrreader
	install -d debian/qrreader/usr/lib/systemd/user
	install -d debian/qrreader/etc/qrreader
	install -m 755 dist/qrreader debian/qrreader/usr/bin/qrreader
	install -m 644 VERSION debian/qrreader/usr/share/qrreader/VERSION
	install -m 644 qrreader.service debian/qrreader/usr/lib/systemd/user/qrreader.service
	install -m 644 debian/default/port debian/qrreader/etc/qrreader/port
	install -m 644 debian/default/update.url debian/qrreader/etc/qrreader/update.url.example

override_dh_shlibdeps:
EOF
}

patch_debian_for_xenial() {
    cp debian/control debian/control.bak.ubuntu16
    cp debian/rules debian/rules.bak.ubuntu16
    sed \
        -e 's/debhelper-compat (= 12)/debhelper (>= 9)/' \
        -e 's/, ${shlibs:Depends}//' \
        -e '/^Rules-Requires-Root:/d' \
        debian/control.bak.ubuntu16 > debian/control
    write_rules_ubuntu16
    echo 9 > debian/compat
    chmod -x debian/install 2>/dev/null || true
    chmod -x debian/qrreader.install 2>/dev/null || true
}

trap restore_debian EXIT INT TERM

install_deb_tools
sync_changelog_from_version
patch_debian_for_xenial

chmod +x dist/qrreader debian/rules

if [ -n "${GIT_TAG:-}" ]; then
    VERSION="${GIT_TAG#v}"
elif [ -n "${RELEASE_TAG:-}" ]; then
    VERSION="${RELEASE_TAG#v}"
else
    VERSION="$(tr -d '\r\n' < VERSION)"
fi
DEB_REV="${DEB_REV:-1}"
DEB_VERSION="${VERSION}-${DEB_REV}"

echo "Packing qrreader_${DEB_VERSION} (Ubuntu 16.04 / debhelper 9) ..."
dpkg-buildpackage -us -uc -b

echo ""
echo "Done:"
ls -la ../*_${DEB_VERSION}_*.deb 2>/dev/null || ls -la ../*.deb 2>/dev/null || true
