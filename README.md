# qrreader-serial

Python and C++ software that reads QR / barcode data from a serial scanner and pastes it into the focused application (clipboard + Ctrl+V).

Current version: see [`VERSION`](VERSION).

## Features

- Reads from a serial barcode / QR scanner
- Copies scanned text to the clipboard and pastes into the active window
- Serbian Cyrillic → Latin transliteration for scanned text
- Clears the clipboard after paste so payment data does not linger
- Ships as a standalone binary / `.deb` (PyInstaller or Linux C++ build)
- Optional systemd user service (`qrreader.service`)

## Quick install (Ubuntu / Debian)

Install the latest GitHub Release `.deb`:

```bash
sudo apt install curl jq
export GITHUB_REPO=grga911/qrreader-serial
chmod +x scripts/install-from-github-release.sh
./scripts/install-from-github-release.sh
systemctl --user enable --now qrreader.service
```

Configure the serial port (default may vary by device):

```bash
sudo mkdir -p /etc/qrreader
echo /dev/ttyACM0 | sudo tee /etc/qrreader/port
systemctl --user restart qrreader.service
```

Follow logs:

```bash
journalctl --user -u qrreader.service -f
```

## Run from source (Python)

Dependencies: Python 3, `pyserial`, `pyperclip`, plus Linux tools `xclip` / `xdotool` (and ideally `xsel`).

```bash
sudo apt install xclip xdotool xsel
python3 -m pip install -r requirements.txt
python3 qrreader.pyw /dev/ttyACM0
```

## Build

| Goal | Docs / command |
|------|----------------|
| PyInstaller binary | [`README_PYINSTALLER.md`](README_PYINSTALLER.md) · `./scripts/build-pyinstaller.sh` |
| `.deb` from PyInstaller binary | [`README_PACKAGING.md`](README_PACKAGING.md) · `./scripts/build-deb.sh` |
| Linux C++ rewrite | [`src/README_linux_cpp.md`](src/README_linux_cpp.md) · `./scripts/build-cpp.sh` |
| GitHub Releases | [`README_GITHUB_RELEASES.md`](README_GITHUB_RELEASES.md) |
| Apt repo on GitHub Pages | [`README_GITHUB_PAGES_APT.md`](README_GITHUB_PAGES_APT.md) |

Tag a release to trigger CI:

```bash
git tag v4.1.6
git push origin v4.1.6
```

## Documentation

Project pages: <https://grga911.github.io/qrreader-serial/>

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Please also follow the [Code of Conduct](CODE_OF_CONDUCT.md).

## Security

To report a vulnerability, see [SECURITY.md](SECURITY.md).

## License

[MIT](LICENSE) — copyright QR Reader contributors.
