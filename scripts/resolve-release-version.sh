#!/bin/sh
# Resolve release version for GitHub Actions.
# Usage: resolve-release-version.sh <changelog-path> [manual-version-input]
#
# Priority:
#   1. Git tag ref (refs/tags/v*)
#   2. Manual workflow_dispatch version input
#   3. VERSION file in repo root
#   4. First version in the given debian changelog
set -eu

CHANGELOG="${1:-debian/changelog}"
MANUAL_VERSION="${2:-}"
REF="${GITHUB_REF:-}"
OUTPUT="${GITHUB_OUTPUT:?GITHUB_OUTPUT must be set}"

if echo "$REF" | grep -qE 'refs/tags/v'; then
    GIT_TAG="${GITHUB_REF_NAME:?GITHUB_REF_NAME must be set for tag builds}"
    echo "Resolved from git tag: $GIT_TAG"
elif [ -n "$MANUAL_VERSION" ]; then
    case "$MANUAL_VERSION" in
        v*) GIT_TAG="$MANUAL_VERSION" ;;
        *) GIT_TAG="v$MANUAL_VERSION" ;;
    esac
    echo "Resolved from workflow input: $GIT_TAG"
elif [ -f VERSION ]; then
    GIT_TAG="v$(tr -d '\r\n' < VERSION)"
    echo "Resolved from VERSION file: $GIT_TAG"
elif [ -f "$CHANGELOG" ]; then
    VERSION="$(sed -n '1s/^.*(\([0-9][0-9.]*\)-.*/\1/p' "$CHANGELOG")"
    if [ -z "$VERSION" ]; then
        echo "Could not parse version from $CHANGELOG" >&2
        exit 1
    fi
    GIT_TAG="v${VERSION}"
    echo "Resolved from $CHANGELOG: $GIT_TAG"
else
    echo "Cannot resolve version: no tag, input, VERSION file, or $CHANGELOG" >&2
    exit 1
fi

VERSION="${GIT_TAG#v}"
{
    printf 'git_tag=%s\n' "$GIT_TAG"
    printf 'version=%s\n' "$VERSION"
    printf 'deb_version=%s-1\n' "$VERSION"
} >> "$OUTPUT"

echo "Release version: $VERSION (deb ${VERSION}-1)"
