#!/bin/sh
# Rebuilds the z80-run PGO training corpus: CoreMark built by this
# repository's clang at -O0, -O2 and -Os and by SDCC, for the Z80 and the
# SM83. The programs are stored in the repository, and the profile is not,
# since only the profile depends on the host compiler.
#
# Usage: build-corpus.sh <llvm build directory>

set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../../.." && pwd)
build=$(cd "${1:?usage: build-corpus.sh <llvm build directory>}" && pwd)
coremark=$root/z80-test/vendor/coremark
sources="core_list_join.c core_main.c core_matrix.c core_state.c core_util.c"

if [ ! -f "$coremark/core_main.c" ]; then
  echo "build-corpus.sh: CoreMark is missing; run" >&2
  echo "  git submodule update --init --depth 1 z80-test/vendor/coremark" >&2
  exit 1
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

for cpu in z80 sm83; do
  for opt in O0 O2 Os; do
    set --
    for src in $sources; do
      set -- "$@" "$coremark/$src"
    done
    "$build/bin/clang" --target=$cpu -$opt -ffreestanding \
      -Dmain=coremark_main -I"$here/coremark" -I"$coremark" \
      "$@" "$here/coremark/core_portme.c" -o "$here/coremark-$cpu-$opt.elf"
  done
done

for cpu in z80 sm83; do
  out=$work/$cpu
  mkdir -p "$out"
  rels=""
  for src in $sources "$here/coremark/core_portme.c"; do
    case $src in
      /*) path=$src ;;
      *) path=$coremark/$src ;;
    esac
    rel=$(basename "$src" .c).rel
    sdcc -m$cpu --std-c11 --opt-code-size -Dmain=coremark_main \
      -I"$here/coremark" -I"$coremark" -c "$path" -o "$out/$rel"
    rels="$rels $rel"
  done
  # sdld replaces whatever follows the last dot in the output path, even in a
  # directory name, so the link runs inside the work directory.
  # shellcheck disable=SC2086
  (cd "$out" && sdcc -m$cpu $rels -o coremark.ihx)
  cp "$out/coremark.ihx" "$here/coremark-sdcc-$cpu.ihx"
done
