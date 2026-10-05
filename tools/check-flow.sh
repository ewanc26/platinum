#!/usr/bin/env bash
# Repository flow checks. Run by .github/workflows/flow.yml and runnable locally.
#
#   tools/check-flow.sh drift                 tree is consistent with its docs and CI
#   tools/check-flow.sh title "<pr title>"    conventional-commit PR title
#   tools/check-flow.sh body <file>           PR body has the template's sections
#   tools/check-flow.sh commits <range>       every commit subject is conventional
#   tools/check-flow.sh release <tag>         tag matches package.json and CHANGELOG
#
# Exit status is non-zero on the first failed group; every violation in that
# group is printed first.
set -uo pipefail
cd "$(dirname "$0")/.."

CC_RE='^(feat|fix|docs|refactor|test|ci|chore|build|perf|revert)(\([a-z0-9._-]+\))?!?: .+'
fail=0
bad() { echo "FAIL: $*"; fail=1; }

cmd_title() {
  [[ "${1:-}" =~ $CC_RE ]] || bad "title is not a conventional commit: '${1:-}'"
}

cmd_commits() {
  local s
  while IFS=$'\t' read -r sha subj; do
    # Merge commits made by GitHub are exempt; everything else must conform.
    [[ "$subj" =~ ^([Mm]erge[: ]) ]] && continue
    [[ "$subj" =~ $CC_RE ]] || bad "commit $sha: '$subj' is not conventional"
  done < <(git log --format='%h%x09%s' "$1")
}

cmd_body() {
  local f=$1 h
  for h in "## What this changes" "## Why" "## Verification"; do
    grep -qxF "$h" "$f" || bad "PR body is missing the '$h' section"
  done
  grep -qiE 'does not prove|not a codewarrior|not hardware|not verified|unverified|not run|not proven' "$f" \
    || bad "PR body must say what the verification does not prove (see Verification in the template)"
}

cmd_release() {
  local tag=${1#v} have
  have=$(sed -n 's/.*"version": *"\([^"]*\)".*/\1/p' bridge/package.json | head -1)
  [ "$have" = "$tag" ] || bad "tag $tag != bridge/package.json version $have"
  grep -q "^## \[$tag\]" CHANGELOG.md || bad "no CHANGELOG.md section for $tag"
}

cmd_drift() {
  local f ref v
  # 1. Every Mac source is in the CI compile list, so a new file cannot dodge the C89 job.
  for f in macos9/*.c macos9/test/*.c; do
    grep -qF "$f" .github/workflows/ci.yml || bad "$f is not compiled by .github/workflows/ci.yml"
  done
  # 2. Every file listed in CI exists.
  while read -r f; do
    [ -f "$f" ] || bad "ci.yml lists $f, which does not exist"
  done < <(grep -o 'macos9/[A-Za-z0-9_/.-]*\.c' .github/workflows/ci.yml | sort -u)
  # 3. AGENTS.md names every Mac source.
  for f in macos9/*.c; do
    grep -q "$(basename "$f")" AGENTS.md || bad "AGENTS.md repository structure does not list $(basename "$f")"
  done
  # 4. The pinned Wolfram ref is the one the docs state.
  ref=$(sed -n 's/^ *ref: *v\([0-9][0-9.]*\).*/\1/p' .github/workflows/ci.yml | head -1)
  [ -n "$ref" ] || bad "no pinned wolfram ref found in ci.yml"
  for f in README.md AGENTS.md; do
    grep -qF "$ref" "$f" || bad "$f does not mention the pinned Wolfram version $ref"
  done
  # 5. Every workflow job that gates merge is reachable from the gate job.
  for v in bridge macos9 flow; do
    grep -qE "needs:.*\b$v\b" .github/workflows/ci.yml || bad "ci.yml gate job does not wait for job '$v'"
  done
}

case "${1:-}" in
  drift) cmd_drift ;;
  title) cmd_title "${2:-}" ;;
  body) cmd_body "${2:?file}" ;;
  commits) cmd_commits "${2:?range}" ;;
  release) cmd_release "${2:?tag}" ;;
  *) sed -n '2,10p' "$0"; exit 2 ;;
esac
[ "$fail" = 0 ] && echo "ok: ${1}"
exit "$fail"
