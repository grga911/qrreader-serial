# PyInstaller build + systemd user service (Linux)

Standalone `qrreader` executable (no system Python at runtime).

## Prerequisites (build machine)

```bash
sudo apt update
sudo apt install -y python3 python3-venv python3-pip \
  binutils  # for PyInstaller on Linux
```

Runtime on target PC (X11 paste): user must be logged into a graphical session (`DISPLAY=:0`).

## Build

```bash
chmod +x scripts/build-pyinstaller.sh scripts/install-pyinstaller.sh
./scripts/build-pyinstaller.sh
```

What the script does:

1. Creates `.venv-build`
2. `pip install -r requirements-build.txt`
3. `pip uninstall` packages listed in **`non-requirements.txt`** (trim fat from pyautogui deps)
4. `pyinstaller qrreader.spec` → **`dist/qrreader`**

Edit **`non-requirements.txt`** if PyInstaller warns about missing modules — remove that package from the uninstall list.

## Install + systemd user service

**Option A — `.deb` (production):**

```bash
./scripts/build-pyinstaller.sh
./scripts/build-deb.sh
sudo dpkg -i ../qrreader_*_amd64.deb
systemctl --user enable --now qrreader.service
```

See **`README_PACKAGING.md`**.

**Option B — manual install:**

```bash
./scripts/install-pyinstaller.sh
```

- Copies `dist/qrreader` → `/usr/bin/qrreader`
- Installs **`qrreader.service`** → `~/.config/systemd/user/qrreader.service`
- Enables and starts the service for **your user**

### Service file

Project file: **`qrreader.service`**

```ini
ExecStart=/usr/local/bin/qrreader
Environment=DISPLAY=:0
Restart=on-failure
```

### Useful commands

```bash
systemctl --user status qrreader.service
systemctl --user restart qrreader.service
systemctl --user stop qrreader.service
journalctl --user -u qrreader.service -f
```

### COM port

Default `/dev/ttyACM0`. Override:

```bash
mkdir -p ~/.config/qrreader
echo /dev/ttyUSB0 > ~/.config/qrreader/port
systemctl --user restart qrreader.service
```

Or `/etc/qrreader/port` (if readable by your user).

## `qrreader.spec` notes

- **Linux:** `console=True` (logs in journalctl)
- **Windows:** `console=False` when built on Windows (same spec detects platform)
- **Icon:** `readerCOM21.ico` only if present in project root
- **excludes / hiddenimports:** tuned for serial + clipboard + X11 hotkey

## Files

| File | Role |
|------|------|
| `requirements.txt` | Runtime pip deps |
| `requirements-build.txt` | Runtime + PyInstaller |
| `non-requirements.txt` | Packages to remove from venv before build |
| `qrreader.spec` | PyInstaller recipe |
| `qrreader.service` | systemd user unit template |
