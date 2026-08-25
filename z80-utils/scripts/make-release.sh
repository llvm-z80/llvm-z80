#!/usr/bin/env bash
#
# Package a 1-stage Release build of the Z80/SM83 LLVM cross toolchain into a
# tarball, the same kind of artifact as an official LLVM release (clang+llvm
# tarball), tailored to the Z80/SM83 target.
#
# Usage:
#   z80-utils/scripts/make-release.sh [TAG] [PLATFORM]
#
#   TAG       release tag, e.g. llvmz80-22.1.7-r1 (default: from `git describe`)
#   PLATFORM  e.g. x86_64-linux (default: derived from uname)
#
# Env overrides:
#   BUILD_DIR   build directory (default: <repo>/build-release)
#   OUT_DIR     where the tarball lands (default: <repo>/z80-utils/release)
#   JOBS        ninja parallelism (default: half the cores; 0 = ninja default)
#
# Produces: <OUT_DIR>/<TAG>-<PLATFORM>.tar.xz

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-release}"

# Default to half the cores: a full-parallel LLVM link step can exhaust RAM.
# Override with JOBS=N (JOBS=0 lets ninja pick its own default).
NCPU="$(nproc 2>/dev/null || echo 2)"
JOBS="${JOBS:-$(( NCPU / 2 > 0 ? NCPU / 2 : 1 ))}"
OUT_DIR="${OUT_DIR:-$REPO_ROOT/z80-utils/release}"

if [ $# -ge 1 ]; then
  TAG="$1"
else
  # An exact llvmz80-* tag on HEAD names a release. Anything else is a snapshot
  # and gets the commit spelled out, so it can never be mistaken for a release.
  TAG="$(git -C "$REPO_ROOT" describe --tags --exact-match --match 'llvmz80-*' 2>/dev/null || true)"
  if [ -z "$TAG" ]; then
    BASE="$(git -C "$REPO_ROOT" describe --tags --match 'llvmz80-*' --abbrev=0 2>/dev/null || echo llvmz80-unknown)"
    TAG="$BASE-g$(git -C "$REPO_ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    echo ">> HEAD is not on an llvmz80-* tag; using snapshot name $TAG"
  fi
fi
PLATFORM="${2:-$(uname -m)-$(uname -s | tr '[:upper:]' '[:lower:]')}"
PKG="${TAG}-${PLATFORM}"
STAGE="$OUT_DIR/$PKG"

echo ">> Packaging $PKG"
echo ">> repo:   $REPO_ROOT"
echo ">> build:  $BUILD_DIR"
echo ">> output: $OUT_DIR"

# 1. Configure (1-stage Release, official-like). Re-runs are cheap.
cmake -G Ninja -S "$REPO_ROOT/llvm" -B "$BUILD_DIR" \
  -C "$REPO_ROOT/clang/cmake/caches/Z80Release.cmake"

# A -C cache file does not overwrite entries an existing CMakeCache already
# has, so a build dir configured before Z80Release.cmake changed keeps the old
# settings. Optional host libraries are what makes a tarball non-portable, so
# check those explicitly rather than shipping a silently stale build.
for var in LLVM_ENABLE_LIBXML2 LLVM_ENABLE_ZLIB LLVM_ENABLE_ZSTD; do
  value="$(sed -nE "s/^$var:[A-Z]+=(.*)$/\1/p" "$BUILD_DIR/CMakeCache.txt")"
  if [ "$value" != "OFF" ]; then
    echo "!! $BUILD_DIR has $var=$value, but this release must be built with it OFF." >&2
    echo "!! Its CMakeCache predates the current Z80Release.cmake." >&2
    echo "!! Remove the build directory and re-run:  rm -rf $BUILD_DIR" >&2
    exit 1
  fi
done

# 2. Build. The Z80Runtime (lib/z80, lib/sm83) is an ALL target, so it builds
#    here too. SDCC-format (.rel/.lib) is added only if sdasz80/sdasgb/sdar
#    are on PATH at configure time.
ninja -C "$BUILD_DIR" $([ "$JOBS" != 0 ] && echo -j"$JOBS")

# 3. Install to a clean staging dir named after the package.
mkdir -p "$OUT_DIR"
rm -rf "$STAGE"
cmake --install "$BUILD_DIR" --prefix "$STAGE"

# 4. Tarball (xz, like the official clang+llvm tarballs).
rm -f "$OUT_DIR/$PKG.tar.xz"
tar -caf "$OUT_DIR/$PKG.tar.xz" -C "$OUT_DIR" "$PKG"
rm -rf "$STAGE"

echo ">> Created $OUT_DIR/$PKG.tar.xz"
echo ">> Contents:"
tar tf "$OUT_DIR/$PKG.tar.xz" | grep -E "bin/(clang|llc|ld\.lld)$|lib/(z80|sm83)/" | sed 's/^/     /' || true
sha256sum "$OUT_DIR/$PKG.tar.xz" | tee "$OUT_DIR/$PKG.tar.xz.sha256"
