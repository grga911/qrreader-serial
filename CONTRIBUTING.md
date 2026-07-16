# Contributing to qrreader-serial

Thanks for helping improve qrreader-serial. By participating, you agree to follow
the [Code of Conduct](CODE_OF_CONDUCT.md).

## Ways to contribute

- Report bugs and propose features with the issue templates
- Improve documentation (README, packaging guides, C++ notes)
- Fix bugs or add features in the Python (`qrreader.pyw`) or C++ (`src/`) trees
- Improve packaging, CI, or install scripts under `scripts/` and `.github/`

## Before you start

1. Search [existing issues](https://github.com/grga911/qrreader-serial/issues) and pull requests.
2. For larger changes, open an issue first so we can agree on the approach.
3. Security issues: do **not** open a public issue — follow [SECURITY.md](SECURITY.md).

## Development setup

### Python path

```bash
sudo apt install xclip xdotool xsel
python3 -m pip install -r requirements.txt
python3 qrreader.pyw /dev/ttyACM0
```

### C++ path

```bash
sudo apt install -y build-essential cmake xclip xdotool
./scripts/build-cpp.sh
./scripts/run-tests.sh
```

See [`src/README_linux_cpp.md`](src/README_linux_cpp.md) for static linking and packaging notes.

### Packaging

- PyInstaller: [`README_PYINSTALLER.md`](README_PYINSTALLER.md)
- Debian package: [`README_PACKAGING.md`](README_PACKAGING.md)

## Pull request checklist

- Keep changes focused; prefer small PRs
- Update docs when behavior or install steps change
- If you change the release version, keep `VERSION` and `debian/changelog` in sync
- Run relevant tests (`./scripts/run-tests.sh` for C++)
- Fill out the pull request template

## Commit style

Prefer [Conventional Commits](https://www.conventionalcommits.org/) where practical:

- `feat:` new behavior
- `fix:` bug fix
- `docs:` documentation only
- `chore:` tooling / packaging / CI
- `refactor:` internal change without user-facing behavior change
- `test:` tests only

## Reporting bugs

Include:

- OS and version (e.g. Ubuntu 22.04)
- Install method (`.deb`, binary, Python source, C++ build)
- Serial device path and scanner model if known
- Steps to reproduce and expected vs actual behavior
- Relevant log lines (`journalctl --user -u qrreader.service`)

## License

Contributions are licensed under the project [MIT License](LICENSE).
