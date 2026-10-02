#!/usr/bin/env bash
#
# Usage: z80-utils/scripts/make-release.sh [TAG] [PLATFORM]

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-release}"

# Full parallelism can run out of memory while linking.
NCPU="$(nproc 2>/dev/null || echo 2)"
JOBS="${JOBS:-$(( NCPU / 2 > 0 ? NCPU / 2 : 1 ))}"
OUT_DIR="${OUT_DIR:-$REPO_ROOT/z80-utils/release}"

if [ $# -ge 1 ]; then
  TAG="$1"
else
  # A snapshot carries its commit so it is never mistaken for a release.
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

cmake -G Ninja -S "$REPO_ROOT/llvm" -B "$BUILD_DIR" \
  -C "$REPO_ROOT/clang/cmake/caches/Z80Release.cmake"

# -C does not override an existing cache, so a stale build dir would keep
# host libraries that make the tarball non-portable.
for var in LLVM_ENABLE_LIBXML2 LLVM_ENABLE_ZLIB LLVM_ENABLE_ZSTD LLVM_ENABLE_LIBEDIT; do
  value="$(sed -nE "s/^$var:[A-Z]+=(.*)$/\1/p" "$BUILD_DIR/CMakeCache.txt")"
  if [ "$value" != "OFF" ]; then
    echo "!! $BUILD_DIR has $var=$value, but this release must be built with it OFF." >&2
    echo "!! Its CMakeCache predates the current Z80Release.cmake." >&2
    echo "!! Remove the build directory and re-run:  rm -rf $BUILD_DIR" >&2
    exit 1
  fi
done

# The SDCC-format runtime is built only if sdasz80, sdasgb and sdar were on
# PATH at configure time.
ninja -C "$BUILD_DIR" $([ "$JOBS" != 0 ] && echo -j"$JOBS")

mkdir -p "$OUT_DIR"
rm -rf "$STAGE"
cmake --install "$BUILD_DIR" --prefix "$STAGE"

rm -f "$OUT_DIR/$PKG.tar.xz"
tar -caf "$OUT_DIR/$PKG.tar.xz" -C "$OUT_DIR" "$PKG"
rm -rf "$STAGE"

echo ">> Created $OUT_DIR/$PKG.tar.xz"
echo ">> Contents:"
tar tf "$OUT_DIR/$PKG.tar.xz" | grep -E "bin/(clang|llc|ld\.lld)$|lib/(z80|sm83)/" | sed 's/^/     /' || true
sha256sum "$OUT_DIR/$PKG.tar.xz" | tee "$OUT_DIR/$PKG.tar.xz.sha256"
