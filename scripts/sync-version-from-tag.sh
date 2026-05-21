#!/bin/sh
# Sync VERSION + debian/changelog from git tag (e.g. v4.0.1 → 4.0.1-1).
# Usage: ./scripts/sync-version-from-tag.sh v4.0.1
set -e
cd "$(dirname "$0")/.."

GIT_TAG="${1:-${GIT_TAG:-}}"
if [ -z "$GIT_TAG" ]; then
    echo "Usage: $0 v4.0.1   (or set GIT_TAG=v4.0.1)"
    exit 1
fi

VERSION="${GIT_TAG#v}"
DEB_REV="${DEB_REV:-1}"
DEB_VERSION="${VERSION}-${DEB_REV}"

printf '%s\n' "$VERSION" > VERSION

{
    printf 'qrreader (%s) unstable; urgency=medium\n\n' "$DEB_VERSION"
    printf '  * Release %s\n\n' "$GIT_TAG"
    printf ' -- QR Reader Maintainer <qrreader@localhost>  %s\n\n' "$(date -R)"
    if [ -f debian/changelog ]; then
        # Keep previous changelog entries below the first blank line block — skip first stanza
        awk 'BEGIN{skip=1} /^$/ && skip {skip=0; next} !skip' debian/changelog 2>/dev/null || true
    fi
} > debian/changelog.new
mv debian/changelog.new debian/changelog

echo "VERSION=$VERSION  deb=${DEB_VERSION}  tag=$GIT_TAG"
