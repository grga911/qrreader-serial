# GitHub Releases + Ubuntu updates

Yes — you can publish builds on **GitHub Releases** and have Ubuntu machines install or upgrade from there.

## Publish a release (CI)

1. Bump `VERSION` and `debian/changelog`.
2. Commit, tag, push:

```bash
git tag v1.0.0
git push origin v1.0.0
```

3. GitHub Actions (`.github/workflows/release.yml`) builds on `ubuntu-22.04` and uploads:
   - `qrreader_<version>-1_amd64.deb`
   - `qrreader-<version>-linux-amd64` (standalone binary)

**Before first run:** edit the workflow if your tag format differs; ensure the repo has Actions enabled.

Replace `owner/repo` below with your real GitHub path (e.g. `mycompany/QR-READER-U22`).

---

## Ubuntu: install / upgrade from GitHub (simple)

On each PC (one-time deps):

```bash
sudo apt install curl jq
```

**Install latest release:**

```bash
export GITHUB_REPO=owner/repo
chmod +x scripts/install-from-github-release.sh
./scripts/install-from-github-release.sh
systemctl --user enable --now qrreader.service
```

**Upgrade to latest** (same command):

```bash
./scripts/install-from-github-release.sh
systemctl --user restart qrreader.service
```

**Specific version:**

```bash
./scripts/install-from-github-release.sh v1.0.0
```

This downloads the `.deb` from the release assets and runs `sudo dpkg -i`.

---

## Version check only (`--check-update`)

If the installed binary supports it, point at the `VERSION` file on GitHub:

```bash
export GITHUB_REPO=owner/repo
./scripts/configure-update-url.sh
```

Creates `/etc/qrreader/update.url` →  
`https://raw.githubusercontent.com/owner/repo/main/VERSION`

Then:

```bash
/usr/bin/qrreader --check-update
```

Exit code `10` = newer version on GitHub (upgrade with the install script above).

---

## Full `apt update && apt upgrade` from GitHub?

GitHub Releases are **not** a native apt repository. Options:

| Method | `apt upgrade` | Complexity |
|--------|----------------|------------|
| **Install script** (above) | No — run script after each release | Low |
| **GitHub Pages apt repo** | Yes | Medium (generate `Packages` / `Release`, host on `gh-pages`) |
| **Packagecloud / Aptly / Nexus** | Yes | Higher |

For most teams: **GitHub Release + `install-from-github-release.sh`** is enough.  
For real `apt upgrade`, host a small apt repo (GitHub Pages or internal HTTP) and point clients at:

```text
deb [trusted=yes] https://your.pages.url ./ 
```

Use `scripts/setup-apt-repo.sh` locally to build that structure, then upload the `apt-repo/` folder to GitHub Pages or a server.

---

## Optional: cron auto-upgrade

```bash
# /etc/cron.weekly/qrreader-upgrade (example — test manually first)
GITHUB_REPO=owner/repo /opt/qrreader/install-from-github-release.sh
```

Ship the script to `/opt/qrreader/` on managed machines.

---

## Summary

| Goal | How |
|------|-----|
| Host builds | Git tag → GitHub Actions → Release assets |
| Ubuntu install | `install-from-github-release.sh` |
| Ubuntu upgrade | Same script again |
| Notify only | `configure-update-url.sh` + `qrreader --check-update` |
| True apt repo | GitHub Pages + `setup-apt-repo.sh` (separate setup) |
