#!/bin/sh
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

tmp_old=debian/changelog.old
[ -f debian/changelog ] && mv debian/changelog "$tmp_old" || : 

{
    printf 'qrreader (%s) stable; urgency=medium\n\n' "$DEB_VERSION"
    printf '  * Release %s\n\n' "$GIT_TAG"
    printf ' -- QR Reader Maintainer <qrreader@localhost>  %s\n\n' "$(date -R)"
    [ -f "$tmp_old" ] && cat "$tmp_old"
} > debian/changelog

rm -f "$tmp_old"

echo "VERSION=$VERSION  deb=${DEB_VERSION}  tag=$GIT_TAG"