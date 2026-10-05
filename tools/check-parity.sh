#!/usr/bin/env bash
# Builds docs/PARITY.md from docs/parity.tsv and checks the rows against the code.
#
#   tools/check-parity.sh          verify (CI): evidence holds and PARITY.md is current
#   tools/check-parity.sh --write  regenerate docs/PARITY.md
#
# An "implemented" row must name a file and a string that is really in it, so a
# feature cannot be claimed without code, and deleting the code breaks the row.
# An "issue" row must name an issue number; when GITHUB_TOKEN is set the issue
# is looked up and must be open. An "impossible" row must carry a reason.
set -uo pipefail
cd "$(dirname "$0")/.."
tsv=docs/parity.tsv
out=$(mktemp)
fail=0
bad() { echo "FAIL: $*"; fail=1; }

n_impl=0; n_issue=0; n_imp=0
{
  cat <<'HDR'
# Parity with Cobalt and Indigo

Generated from [parity.tsv](parity.tsv) by `tools/check-parity.sh --write`. Do
not edit by hand: CI regenerates it and fails on a difference.

The Platinum column is checked against the code. An implemented row names a file
and a string that has to be in it. An open row links the issue that tracks it.
The Cobalt and Indigo columns are read from their READMEs and source trees on
2026-10-05 and are not checked by this repository's CI.

Mac OS 9 gaps are designed for the platform (bounded memory, cooperative event
loop, 640x480, no decoding on the Mac) rather than ported from the consoles.

| Feature | Cobalt | Indigo | Platinum |
| --- | --- | --- | --- |
HDR
} > "$out"

while IFS='|' read -r id feature cobalt indigo status evidence; do
  [[ -z "$id" || "$id" == \#* ]] && continue
  case "$status" in
    implemented)
      file=${evidence%%::*}; needle=${evidence#*::}
      if [ ! -f "$file" ]; then bad "$id: $file does not exist"
      elif ! grep -qF -- "$needle" "$file"; then bad "$id: '$needle' not found in $file"; fi
      cell="implemented (\`$file\`)"; n_impl=$((n_impl+1)) ;;
    issue)
      [[ "$evidence" =~ ^#[0-9]+$ ]] || bad "$id: evidence '$evidence' is not an issue number"
      if [ -n "${GITHUB_TOKEN:-}" ] && [[ "$evidence" =~ ^#([0-9]+)$ ]]; then
        st=$(curl -fsS -H "Authorization: Bearer $GITHUB_TOKEN" \
          "https://api.github.com/repos/${GITHUB_REPOSITORY:-ewanc26/platinum}/issues/${BASH_REMATCH[1]}" \
          | sed -n 's/^  "state": "\([a-z]*\)".*/\1/p')
        [ "$st" = open ] || bad "$id: $evidence is '${st:-unknown}', not open; mark the row implemented or file a new issue"
      fi
      cell="[$evidence](https://github.com/ewanc26/platinum/issues/${evidence#\#})"; n_issue=$((n_issue+1)) ;;
    impossible)
      [ ${#evidence} -ge 20 ] || bad "$id: impossible needs a reason with a source"
      cell="not possible: $evidence"; n_imp=$((n_imp+1)) ;;
    *) bad "$id: unknown status '$status'"; cell="?" ;;
  esac
  [[ "$cobalt" =~ ^(yes|no)$ && "$indigo" =~ ^(yes|no)$ ]] || bad "$id: cobalt/indigo must be yes or no"
  echo "| $feature | $cobalt | $indigo | $cell |" >> "$out"
done < "$tsv"
printf '\n%d implemented, %d tracked by an open issue, %d not possible.\n' \
  "$n_impl" "$n_issue" "$n_imp" >> "$out"

if [ "${1:-}" = "--write" ]; then
  cp "$out" docs/PARITY.md; echo "wrote docs/PARITY.md"
elif ! diff -u docs/PARITY.md "$out" >/dev/null; then
  bad "docs/PARITY.md is stale; run tools/check-parity.sh --write"
  diff -u docs/PARITY.md "$out" | head -20
fi
rm -f "$out"
[ "$fail" = 0 ] && echo "ok: parity"
exit "$fail"
