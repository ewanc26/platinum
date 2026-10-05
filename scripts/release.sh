#!/usr/bin/env bash
# Cut a Platinum release: the inputs the bridge updater consumes.
#
#   scripts/release.sh [--dry-run] <MAJOR.MINOR.PATCH>
#
# Builds release/platinum-bridge-<v>.tar.gz (dist/, package.json and production
# node_modules, so an update runs nothing from a registry), <archive>.sha256 and
# update.json. Without --dry-run it also tags v<v> and attaches those
# three files to a GitHub release. It publishes to no package registry.
#
# Refuses unless the tree is clean, you are on main, main equals origin/main,
# bridge/package.json already says <v>, and CHANGELOG.md has a "## [<v>]"
# section. The manifest URLs point at this repository's release downloads and
# the updater refuses anything else.
set -euo pipefail

dry=0
if [[ "${1:-}" == "--dry-run" ]]; then dry=1; shift; fi
version="${1:-}"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "usage: $0 [--dry-run] <MAJOR.MINOR.PATCH>" >&2; exit 2; }
tag="v$version"
cd "$(git rev-parse --show-toplevel)"
fail() { echo "release: $*" >&2; exit 1; }

if [ "$dry" = 0 ] && [ -e RELEASES_PAUSED ]; then
  fail "releases are paused (RELEASES_PAUSED, issue #62); only --dry-run is allowed"
fi
if [ "$dry" = 0 ]; then
  [[ "$(git rev-parse --abbrev-ref HEAD)" == main ]] || fail "must be on main"
  [[ -z "$(git status --porcelain)" ]] || fail "working tree is not clean"
  git fetch origin --tags --prune
  [[ "$(git rev-parse main)" == "$(git rev-parse origin/main)" ]] || fail "local main differs from origin/main"
  git rev-parse -q --verify "refs/tags/$tag" >/dev/null && fail "tag $tag already exists"
fi
tools/check-flow.sh release "$tag" || fail "tag/version/changelog mismatch"

out=release
rm -rf "$out" "$out.staging"
mkdir -p "$out" "$out.staging"

echo "release: checks and build"
( cd bridge && npm install --no-audit --no-fund && npm run check && npm test && npm run build )

echo "release: staging bridge with production dependencies only"
cp -R bridge/dist bridge/package.json "$out.staging/"
( cd "$out.staging" && npm install --omit=dev --ignore-scripts --no-audit --no-fund >/dev/null && rm -f package-lock.json )

archive="platinum-bridge-$version.tar.gz"
# Fixed ordering and ownership so the same tree gives the same bytes.
tar --sort=name --owner=0 --group=0 --numeric-owner --mtime='2026-01-01 00:00Z' \
  -czf "$out/$archive" -C "$out.staging" .
rm -rf "$out.staging"

sha="$(sha256sum "$out/$archive" | cut -d' ' -f1)"
size="$(wc -c < "$out/$archive" | tr -d ' ')"
notes="$(awk -v v="$version" '$0 ~ "^## \\[" v "\\]" {f=1; next} f && /^## \[/ {exit} f {print}' CHANGELOG.md | tr '\n' ' ' | cut -c1-1000 | tr -d '"\\')"
cat > "$out/update.json" <<JSON
{
  "schema": 1,
  "app": "platinum-bridge",
  "version": "$version",
  "notes": "$notes",
  "asset": {
    "name": "$archive",
    "url": "https://github.com/ewanc26/platinum/releases/download/$tag/$archive",
    "size": $size,
    "sha256": "$sha"
  },
  "signature": null
}
JSON
echo "$sha  $archive" > "$out/$archive.sha256"
echo "release: wrote $out/ (archive, .sha256, update.json)"

if [ "$dry" = 1 ]; then echo "release: dry run, nothing tagged or published"; exit 0; fi

notes="$(awk -v v="$version" '$0 ~ "^## \\[" v "\\]" {f=1; next} f && /^## \[/ {exit} f {print}' CHANGELOG.md)"
git tag -a "$tag" -m "Platinum $version" "$(git rev-parse main)"
git push origin "$tag"
gh release create "$tag" "$out/$archive" "$out/$archive.sha256" "$out/update.json" \
  --title "Platinum $version" --notes "$notes"
