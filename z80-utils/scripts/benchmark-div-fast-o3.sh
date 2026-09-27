#!/bin/bash
# SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
#
# Benchmark: 16-Bit Division/Modulo Performance (-O2 vs -O3)
#
# Compares CPU clock cycles (T-states) of 16-bit division and modulo routines
# between size-focused optimization (-O2, ___udivhi3/___divhi3) and
# speed-focused aggressive optimization (-O3, ___udivhi3_fast/___divhi3_fast).
#
# Usage:
#   ./z80-utils/scripts/benchmark-div-fast-o3.sh
#   BUILD_DIR=/path/to/build ./z80-utils/scripts/benchmark-div-fast-o3.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Locate LLVM Z80 build directory
if [ -n "$BUILD_DIR" ]; then
    :
elif [ -d "$REPO_DIR/build-macos/lib/z80/elf-runtime" ] && [ -x "$REPO_DIR/build-macos/bin/clang" ]; then
    BUILD_DIR="$REPO_DIR/build-macos"
elif [ -d "$REPO_DIR/build/lib/z80/elf-runtime" ] && [ -x "$REPO_DIR/build/bin/clang" ]; then
    BUILD_DIR="$REPO_DIR/build"
else
    echo "ERROR: Could not locate LLVM Z80 build directory with elf-runtime" >&2
    exit 1
fi

if ! command -v z88dk-ticks >/dev/null 2>&1; then
    echo "ERROR: z88dk-ticks simulator not found on PATH" >&2
    exit 1
fi

CLANG="$BUILD_DIR/bin/clang"
LLD="$BUILD_DIR/bin/ld.lld"
OBJCOPY="$BUILD_DIR/bin/llvm-objcopy"
NM="$BUILD_DIR/bin/llvm-nm"
STAGE="$BUILD_DIR/lib/z80/elf-runtime"
CRT0="$STAGE/crt0.o"
LDSCRIPT="$BUILD_DIR/lib/z80/z80.ld"

if [ ! -f "$CRT0" ] || [ ! -f "$LDSCRIPT" ]; then
    echo "ERROR: runtime crt0 or linker script missing in $STAGE" >&2
    exit 1
fi

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

BENCH_C="$TMPDIR/bench.c"
cat << 'C_EOF' > "$BENCH_C"
typedef unsigned short uint16_t;
typedef short int16_t;

__attribute__((noinline)) uint16_t test_udiv(uint16_t a, uint16_t b) { return a / b; }
__attribute__((noinline)) uint16_t test_umod(uint16_t a, uint16_t b) { return a % b; }
__attribute__((noinline)) int16_t  test_sdiv(int16_t a, int16_t b)   { return a / b; }
__attribute__((noinline)) int16_t  test_smod(int16_t a, int16_t b)   { return a % b; }

int main(void) {
    uint16_t u = 0;
    int16_t s = 0;
    for (uint16_t i = 1; i <= 100; i++) {
        uint16_t num = i * 6 + (i % 5);
        uint16_t den = i * 2 + 1;
        u += test_udiv(num, den);
        u += test_umod(num, den);
        s += test_sdiv((int16_t)num, (int16_t)den);
        s += test_smod((int16_t)num, (int16_t)den);
    }
    return (int)(u + (uint16_t)s);
}
C_EOF

run_opt() {
    local opt="$1"
    "$CLANG" --target=z80 -"$opt" -c -nostdlib -ffreestanding "$BENCH_C" -o "$TMPDIR/test_$opt.o"
    "$LLD" --gc-sections -T "$LDSCRIPT" "$CRT0" "$TMPDIR/test_$opt.o" "$STAGE"/builtins/*.o -o "$TMPDIR/test_$opt.elf"
    "$OBJCOPY" -O binary "$TMPDIR/test_$opt.elf" "$TMPDIR/test_$opt.bin"
    local halt_addr
    halt_addr=$("$NM" "$TMPDIR/test_$opt.elf" | grep " _halt$" | awk '{print "0x"$1}')
    z88dk-ticks -end "$halt_addr" -counter 100000000 -output "$TMPDIR/test_$opt.ram" "$TMPDIR/test_$opt.bin" | tail -n 1
}

O2_CYCLES=$(run_opt O2)
O3_CYCLES=$(run_opt O3)

SAVED=$((O2_CYCLES - O3_CYCLES))
PCT=$(awk "BEGIN {printf \"%.1f\", ($SAVED / $O2_CYCLES) * 100}")
SPEEDUP=$(awk "BEGIN {printf \"%.2f\", ($O2_CYCLES / $O3_CYCLES)}")

echo "### 16-Bit Division/Modulo Performance Measurement (Z80 T-States)"
echo ""
echo "| Opt Level | Runtime Libcalls | Clock Cycles (T-states) | Speedup vs -O2 |"
echo "|:---|:---|:---:|:---:|"
echo "| **-O2** (size-focused) | \`___udivhi3\`, \`___divhi3\` | $O2_CYCLES T-states | Baseline |"
echo "| **-O3** (speed-focused) | \`___udivhi3_fast\`, \`___divhi3_fast\` | $O3_CYCLES T-states | **${SPEEDUP}x faster** (-${PCT}%) |"
echo ""
echo "*Measured on 100 iterations of 16-bit signed/unsigned division and modulo via \`z88dk-ticks\`.*"
