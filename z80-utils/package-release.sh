#!/usr/bin/env bash
#
# Package a 1-stage Release build of the Z80/SM83 LLVM cross toolchain into a
# tarball, the same kind of artifact as an official LLVM release (clang+llvm
# tarball), tailored to the Z80/SM83 target.
#
# Usage:
#   z80-utils/package-release.sh [TAG] [PLATFORM]
#
#   TAG       release tag, e.g. llvmz80-22.1.7-r1 (default: from `git describe`)
#   PLATFORM  e.g. x86_64-linux (default: derived from uname)
#
# Env overrides:
#   BUILD_DIR   build directory (default: <repo>/build-release)
#   JOBS        ninja parallelism
#
# Produces: <TAG>-<PLATFORM>.tar.xz in the repo root.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-release}"

TAG="${1:-$(git -C "$REPO_ROOT" describe --tags --match 'llvmz80-*' --abbrev=0 2>/dev/null || echo llvmz80-unknown)}"
PLATFORM="${2:-$(uname -m)-$(uname -s | tr '[:upper:]' '[:lower:]')}"
PKG="${TAG}-${PLATFORM}"
STAGE="$REPO_ROOT/$PKG"

echo ">> Packaging $PKG"
echo ">> repo:  $REPO_ROOT"
echo ">> build: $BUILD_DIR"

# 1. Configure (1-stage Release, official-like). Re-runs are cheap.
cmake -G Ninja -S "$REPO_ROOT/llvm" -B "$BUILD_DIR" \
  -C "$REPO_ROOT/clang/cmake/caches/Z80Release.cmake"

# 2. Build. The Z80Runtime (lib/z80, lib/sm83) is an ALL target, so it builds
#    here too. SDCC-format (.rel/.lib) is added only if sdasz80/sdasgb/sdar
#    are on PATH at configure time.
ninja -C "$BUILD_DIR" ${JOBS:+-j"$JOBS"}

# 3. Install to a clean staging dir named after the package.
rm -rf "$STAGE"
cmake --install "$BUILD_DIR" --prefix "$STAGE"

# 4. Tarball (xz, like the official clang+llvm tarballs).
rm -f "$REPO_ROOT/$PKG.tar.xz"
tar -caf "$REPO_ROOT/$PKG.tar.xz" -C "$REPO_ROOT" "$PKG"

echo ">> Created $PKG.tar.xz"
echo ">> Contents:"
tar tf "$REPO_ROOT/$PKG.tar.xz" | grep -E "bin/(clang|llc|ld\.lld)$|lib/(z80|sm83)/" | sed 's/^/     /' || true
