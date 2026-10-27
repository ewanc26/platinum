#!/usr/bin/env bash
# Build Wolfram's Node addon (node/wolfram_core.c) and place the module where
# the bridge loads it, bridge/build/wolfram_core.node.
#
# The bridge runs its update contract (manifest parsing, version comparison,
# SHA-256, signature verification) in Wolfram's own C through this addon
# instead of TypeScript ports; see AGENTS.md, section 5. The Wolfram commit is
# pinned in .github/workflows/ci.yml and its docs, and is bumped deliberately.
#
# Usage: tools/build-addon.sh <wolfram-checkout>
# The checkout is configured out of tree into build-wolfram-addon/ (gitignored).

set -euo pipefail

cd "$(dirname "$0")/.."

[ $# -eq 1 ] || { echo "usage: tools/build-addon.sh <wolfram-checkout>" >&2; exit 2; }
WOLFRAM="$1"
[ -f "$WOLFRAM/CMakeLists.txt" ] || { echo "build-addon: $WOLFRAM is not a Wolfram checkout" >&2; exit 2; }

cmake -S "$WOLFRAM" -B build-wolfram-addon -DWOLFRAM_BUILD_NODE=ON
cmake --build build-wolfram-addon --target wolfram_core
mkdir -p bridge/build
cp build-wolfram-addon/wolfram_core.node bridge/build/wolfram_core.node
echo "build-addon: bridge/build/wolfram_core.node is in place"
