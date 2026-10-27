#!/usr/bin/env bash
# Build Platinum's Mac OS 9 application with Retro68.
#
# Retro68 (https://github.com/autc04/Retro68) is a GCC-based cross-compiler
# for Classic Mac OS (68k and PowerPC). The easiest source is the prebuilt
# image, which contains both toolchains:
#
#   docker run --rm -v "$PWD":/root -v /path/to/wolfram:/root/wolfram \
#     -i ghcr.io/autc04/retro68 tools/build-macos9.sh
#
# A locally built toolchain works too; pass its build directory as the
# prefix (where toolchain/m68k-apple-macos/ lives).
#
# Usage: tools/build-macos9.sh [68k|ppc|both] [retro68-build-dir]
# Defaults: both architectures, /Retro68-build (the Docker image's layout).

set -euo pipefail

cd "$(dirname "$0")/.."

ARCH="${1:-both}"
ROOT="${2:-/Retro68-build}"

case "$ARCH" in
    68k|ppc|both) ;;
    *) echo "usage: $0 [68k|ppc|both] [retro68-build-dir]" >&2; exit 2 ;;
esac

TOOLCHAIN_68K="$ROOT/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake"
TOOLCHAIN_PPC="$ROOT/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake"

# Wolfram public headers are needed at compile time. The default matches the
# CI layout, where the Wolfram checkout sits next to (or inside) the repo.
WOLFRAM_INCLUDE_DIR="${WOLFRAM_INCLUDE_DIR:-$PWD/wolfram/include}"
if [ ! -d "$WOLFRAM_INCLUDE_DIR/wolfram" ]; then
    echo "error: Wolfram headers not found at $WOLFRAM_INCLUDE_DIR" >&2
    echo "Set WOLFRAM_INCLUDE_DIR to a Wolfram checkout's include/ directory." >&2
    exit 1
fi

build_arch() {
    local arch=$1 toolchain=$2
    echo "==> Building Platinum for Mac OS 9 ($arch)"
    cmake -S macos9 -B "build-macos9-$arch" \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
        -DWOLFRAM_INCLUDE_DIR="$WOLFRAM_INCLUDE_DIR"
    cmake --build "build-macos9-$arch"
    echo "==> $arch application: build-macos9-$arch/Platinum.bin (MacBinary)"
    echo "==> also: build-macos9-$arch/Platinum.APPL (BasiliskII/SheepShaver)"
}

if [ "$ARCH" = "68k" ] || [ "$ARCH" = "both" ]; then
    [ -f "$TOOLCHAIN_68K" ] || { echo "error: no 68k toolchain at $TOOLCHAIN_68K" >&2; exit 1; }
    build_arch 68k "$TOOLCHAIN_68K"
fi
if [ "$ARCH" = "ppc" ] || [ "$ARCH" = "both" ]; then
    [ -f "$TOOLCHAIN_PPC" ] || { echo "error: no PowerPC toolchain at $TOOLCHAIN_PPC" >&2; exit 1; }
    build_arch ppc "$TOOLCHAIN_PPC"
fi

echo "Done. Copy build-macos9-*/Platinum.bin to a Mac OS 9 volume and run it."
