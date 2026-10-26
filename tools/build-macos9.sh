#!/usr/bin/env bash
# Build Platinum's Mac OS 9 application with Retro68.
#
# Retro68 is a GCC-based cross-compiler for Classic Mac OS (68k and PowerPC).
# It is not packaged by Homebrew or MacPorts; build it from source:
#
#   git clone https://github.com/autc04/Retro68.git
#   cd Retro68 && ./bootstrap && make && sudo make install
#
# Retro68 installs its CMake toolchain files under
#   <prefix>/share/retro68/cmake/Toolchain-retro68-68k.cmake
#   <prefix>/share/retro68/cmake/Toolchain-retro68-powerpc.cmake
#
# Usage:
#   tools/build-macos9.sh [68k|ppc|both] [retro68-prefix]
#
# Defaults: both architectures, prefix /usr/local.

set -euo pipefail

cd "$(dirname "$0")/.."

ARCH="${1:-both}"
PREFIX="${2:-/usr/local}"

case "$ARCH" in
    68k|ppc|both) ;;
    *) echo "usage: $0 [68k|ppc|both] [retro68-prefix]" >&2; exit 2 ;;
esac

TOOLCHAIN_68K="$PREFIX/share/retro68/cmake/Toolchain-retro68-68k.cmake"
TOOLCHAIN_PPC="$PREFIX/share/retro68/cmake/Toolchain-retro68-powerpc.cmake"

if [ ! -f "$TOOLCHAIN_68K" ] && [ ! -f "$TOOLCHAIN_PPC" ]; then
    echo "error: Retro68 not found at $PREFIX" >&2
    echo "Install Retro68 first: git clone https://github.com/autc04/Retro68.git" >&2
    echo "  cd Retro68 && ./bootstrap && make && sudo make install" >&2
    exit 1
fi

# Wolfram public headers are needed at compile time. Look for a sibling
# checkout first, then fall back to WOLFRAM_INCLUDE_DIR.
WOLFRAM_INCLUDE_DIR="${WOLFRAM_INCLUDE_DIR:-}"
if [ -z "$WOLFRAM_INCLUDE_DIR" ]; then
    for d in ../wolfram ../wolfram-main-edab3d6 /Volumes/Storage/Mac/projects/Developer/Git/wolfram*; do
        if [ -d "$d/include/wolfram" ]; then
            WOLFRAM_INCLUDE_DIR="$(cd "$d" && pwd)/include"
            break
        fi
    done
fi
if [ -z "$WOLFRAM_INCLUDE_DIR" ]; then
    echo "error: Wolfram include directory not found. Set WOLFRAM_INCLUDE_DIR." >&2
    exit 1
fi

build_arch() {
    local arch=$1 toolchain=$2
    echo "==> Building Platinum for Mac OS 9 ($arch)"
    cmake -S macos9 -B "build-macos9-$arch" \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
        -DWOLFRAM_INCLUDE_DIR="$WOLFRAM_INCLUDE_DIR"
    cmake --build "build-macos9-$arch"
    echo "==> $arch application: build-macos9-$arch/Platinum"
    file "build-macos9-$arch/Platinum" 2>/dev/null || true
}

if [ "$ARCH" = "68k" ] || [ "$ARCH" = "both" ]; then
    build_arch 68k "$TOOLCHAIN_68K"
fi
if [ "$ARCH" = "ppc" ] || [ "$ARCH" = "both" ]; then
    build_arch ppc "$TOOLCHAIN_PPC"
fi

echo "Done. Copy build-macos9-*/Platinum to a Mac OS 9 volume and run it."
