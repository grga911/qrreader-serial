#!/bin/sh
set -eu

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

export GIT_TAG=v9.9.9
export DEB_REV=1

mkdir -p "$TMP/debian" "$TMP/scripts"
cp "$ROOT/scripts/sync-version-from-tag.sh" "$TMP/scripts/sync-version-from-tag.sh"
chmod +x "$TMP/scripts/sync-version-from-tag.sh"
printf 'qrreader (1.0.0-1) stable; urgency=medium\n\n  * Old entry\n\n' > "$TMP/debian/changelog"

sh "$TMP/scripts/sync-version-from-tag.sh" "$GIT_TAG"

test -f "$TMP/VERSION"
grep -Fq '9.9.9' "$TMP/VERSION"
grep -Fq 'qrreader (9.9.9-1)' "$TMP/debian/changelog"
grep -Fq 'Old entry' "$TMP/debian/changelog"
test ! -f "$TMP/debian/changelog.old"

rm -f "$TMP/debian/changelog"
sh "$TMP/scripts/sync-version-from-tag.sh" "$GIT_TAG"
test -f "$TMP/VERSION"
grep -Fq 'qrreader (9.9.9-1)' "$TMP/debian/changelog"
test ! -f "$TMP/debian/changelog.old"

echo "sync-version-from-tag tests passed"
