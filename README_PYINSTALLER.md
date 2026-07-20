# PyInstaller build

Standalone `qrreader` executable (no system Python at runtime).

| Platform | Output | Build script |
|----------|--------|--------------|
| Linux | `dist/qrreader` | `./scripts/build-pyinstaller.sh` |
| Windows | `dist/qrreader.exe` | `.\scripts\build-pyinstaller.ps1` or `scripts\build-pyinstaller.bat` |

---

## Windows

### Prerequisites (build PC)

- Windows 10/11
- [Python 3](https://www.python.org/downloads/) with **Add python.exe to PATH**
- PowerShell (included with Windows)

### Build

From the repo root in PowerShell:

```powershell
.\scripts\build-pyinstaller.ps1
```

Or double-click / run from cmd:

```bat
scripts\build-pyinstaller.bat
```

What the script does:

1. Creates `.venv-build`
2. `pip install -r requirements-build.txt`
3. `pip uninstall` packages listed in **`non-requirements.txt`**
4. `pyinstaller qrreader.spec` → **`dist\qrreader.exe`**

### Install + autostart

```powershell
.\scripts\install-pyinstaller.ps1
# optional:
.\scripts\install-pyinstaller.ps1 -ComPort COM3
.\scripts\install-pyinstaller.ps1 -NoStartup
```

- Copies `dist\qrreader.exe` → `%LOCALAPPDATA%\qrreader\qrreader.exe`
- Writes port config → `%APPDATA%\qrreader\port` (default `COM21`)
- Adds a Startup-folder shortcut so it runs at login (skip with `-NoStartup`)

### Run without install

```powershell
.\dist\qrreader.exe COM21
python qrreader.pyw COM21
```

### COM port

Default `COM21`. Override:

```powershell
New-Item -ItemType Directory -Force -Path "$env:APPDATA\qrreader" | Out-Null
Set-Content "$env:APPDATA\qrreader\port" "COM3"
```

Or pass the port on the command line: `qrreader.exe COM3`.

The Windows build is **windowless** (`console=False`). To see `--version` / logs, run from a console:

```powershell
& "$env:LOCALAPPDATA\qrreader\qrreader.exe" --version
```

---

## Linux

### Prerequisites (build machine)

Scripts install apt dependencies automatically (via `sudo` when needed), including
`python3`, `python3-venv`, `python3-pip`, and `binutils`. You can also install them
manually:

```bash
sudo apt update
sudo apt install -y python3 python3-venv python3-pip \
  binutils  # for PyInstaller on Linux
```

Runtime on target PC (X11 paste): user must be logged into a graphical session (`DISPLAY=:0`).
Install `xclip` and `xdotool` (included as dependencies in the `.deb` package; also
installed by `scripts/install-pyinstaller.sh`).

### Build

```bash
chmod +x scripts/build-pyinstaller.sh scripts/install-pyinstaller.sh
./scripts/build-pyinstaller.sh
```

What the script does:

1. Creates `.venv-build`
2. `pip install -r requirements-build.txt`
3. `pip uninstall` packages listed in **`non-requirements.txt`** (trim fat from transitive deps)
4. `pyinstaller qrreader.spec` → **`dist/qrreader`**

Edit **`non-requirements.txt`** if PyInstaller warns about missing modules — remove that package from the uninstall list.

### Install + systemd user service

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

---

## `qrreader.spec` notes

- **Linux:** `console=True` (logs in journalctl); `strip=True`
- **Windows:** `console=False` (no black console window); `strip` disabled; UPX enabled when available
- **Icon:** `readerCOM21.ico` only if present in project root
- **VERSION:** bundled into the frozen binary for `--version`
- **excludes / hiddenimports:** tuned for serial + clipboard; paste uses `xdotool` (Linux) or Win32 `keybd_event` (Windows)

## Files

| File | Role |
|------|------|
| `qrreader.pyw` | App entry (windowless on Windows when run as `.pyw`) |
| `requirements.txt` | Runtime pip deps |
| `requirements-build.txt` | Runtime + PyInstaller |
| `non-requirements.txt` | Packages to remove from venv before build |
| `qrreader.spec` | PyInstaller recipe (Linux + Windows) |
| `scripts/build-pyinstaller.sh` | Linux build |
| `scripts/build-pyinstaller.ps1` / `.bat` | Windows build |
| `scripts/install-pyinstaller.sh` | Linux install + systemd |
| `scripts/install-pyinstaller.ps1` / `.bat` | Windows install + Startup |
| `qrreader.service` | systemd user unit template (Linux) |
