# GitHub Pages as an apt repository

Yes — each **Release** workflow run can automatically publish a `.deb` apt repo to the **`gh-pages`** branch. Ubuntu clients then use:

```bash
sudo apt update
sudo apt upgrade qrreader
```

## One-time setup (GitHub)

### 1. Enable GitHub Pages

1. Open the repo on GitHub → **Settings** → **Pages**
2. **Build and deployment** → Source: **Deploy from a branch**
3. Branch: **`gh-pages`** → folder **`/ (root)`**
4. Save

The first successful release workflow creates the `gh-pages` branch. If Pages is empty, run the workflow once (push a tag), then refresh Settings → Pages.

### 2. Workflow (already in repo)

On every tag `v*` (and manual **Release** run), job **`publish-apt-pages`**:

1. Takes the built `.deb` from the build job
2. Runs `scripts/setup-apt-repo.sh` → `Packages`, `Packages.gz`, `Release`
3. Pushes to **`gh-pages`** branch under **`/apt/`** (via `peaceiris/actions-gh-pages`)

No extra secrets needed — `GITHUB_TOKEN` is enough (`contents: write`).

### 3. Automatic?

| Event | GitHub Release assets | GitHub Pages apt repo |
|--------|----------------------|------------------------|
| Push tag `v4.0.1` | Yes | Yes |
| Manual workflow run | Yes | Yes |

---

## Ubuntu client setup

Replace `OWNER` and `REPO` with your GitHub path (same as `github.com/OWNER/REPO`).

```bash
echo "deb [trusted=yes] https://OWNER.github.io/REPO/apt ./" | \
  sudo tee /etc/apt/sources.list.d/qrreader.list

sudo apt update
sudo apt install qrreader
```

**Upgrade** after you publish a new tag:

```bash
sudo apt update
sudo apt upgrade qrreader
systemctl --user restart qrreader.service
```

`[trusted=yes]` is required because the repo is not signed with an apt key (fine for internal use).

### Example

Repo: `https://github.com/myorg/qrreader-serial`

```bash
echo "deb [trusted=yes] https://myorg.github.io/qrreader-serial/apt ./" | \
  sudo tee /etc/apt/sources.list.d/qrreader.list
sudo apt update
sudo apt install qrreader
```

Check the URL in the browser — you should see `Packages`, `Release`, and a `.deb` file.

---

## Verify Pages deploy

After a release workflow:

1. **Actions** → latest **Release** → job **publish-apt-pages** (green)
2. Branch **`gh-pages`** exists with folder **`apt/`**
3. Open: `https://OWNER.github.io/REPO/apt/Packages`

---

## Local test (without GitHub)

```bash
./scripts/build-release.sh
./scripts/setup-apt-repo.sh ./apt-repo ../*.deb

# optional local HTTP test:
cd apt-repo && python3 -m http.server 8080
# another terminal:
# echo "deb [trusted=yes] http://127.0.0.1:8080 ./" | sudo tee /etc/apt/sources.list.d/qrreader-local.list
```

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `404` on Pages URL | Enable Pages; wait 1–2 min after first deploy |
| `apt update` no package | Wrong OWNER/REPO in URL; check `/apt/Packages` in browser |
| `GPG error` | Use `[trusted=yes]` in the `deb` line |
| Only one version in repo | Each release **replaces** `gh-pages/apt/` with the latest build (normal for small teams) |

---

## Also published

The same workflow still uploads files to **GitHub Releases** (download page). Pages apt repo and Releases run in parallel.
