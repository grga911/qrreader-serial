# Packaging: PyInstaller binary → `.deb`

Build the standalone executable **once**, then pack it into a Debian package (no Python on target machines).

## Two-step release (recommended)

### 1. Build executable (PyInstaller)

```bash
./scripts/build-pyinstaller.sh
```

Produces **`dist/qrreader`** (see `README_PYINSTALLER.md` for venv / `non-requirements.txt` details).

Build on the **same CPU architecture** as deployment (e.g. amd64 PC → amd64 `.deb`).

### 2. Pack into `.deb`

```bash
./scripts/build-deb.sh
```

Output (parent folder):

```text
../qrreader_1.0.0-1_amd64.deb
```

### One command (both steps)

```bash
chmod +x scripts/build-release.sh
./scripts/build-release.sh
```

## Install on target PC

```bash
sudo dpkg -i ../qrreader_1.0.0-1_amd64.deb
sudo apt -f install
```

Per **desktop user** (clipboard / `DISPLAY` need a logged-in session):

```bash
systemctl --user daemon-reload
systemctl --user enable --now qrreader.service
journalctl --user -u qrreader.service -f
```

## What the `.deb` contains

| Path | Content |
|------|---------|
| `/usr/bin/qrreader` | PyInstaller binary |
| `/usr/share/qrreader/VERSION` | Version string |
| `/etc/qrreader/port` | Default serial port (conffile) |
| `/usr/lib/systemd/user/qrreader.service` | User systemd unit |

No `python3`, no pip on the target system.

## Configuration

```bash
sudo nano /etc/qrreader/port          # e.g. /dev/ttyACM0
systemctl --user restart qrreader.service
```

## Updates

### GitHub Releases (recommended)

Tag → CI uploads `.deb` → Ubuntu runs `install-from-github-release.sh`.  
See **`README_GITHUB_RELEASES.md`**.

### Private apt repo

1. Bump `VERSION` and `debian/changelog`
2. `./scripts/build-release.sh`
3. Publish new `.deb` to your apt repo (see `scripts/setup-apt-repo.sh`)
4. Clients: `sudo apt update && sudo apt upgrade qrreader`

## Manual install without `.deb`

```bash
./scripts/build-pyinstaller.sh
./scripts/install-pyinstaller.sh
```

Same binary and `qrreader.service`, without dpkg.

## Notes

- **Architecture:** package is `amd64`; build PyInstaller on amd64 (or adjust `debian/control` for arm64 builds).
- **Service:** must run as the **user** who owns the X11/Wayland session (`systemctl --user`, not system-wide).
- Source script `qrreader.pyw` is only for development; production uses `dist/qrreader`.
