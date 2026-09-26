#!/bin/bash
# SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
#
# Benchmark: 32-Bit Division/Modulo Fusion Performance (Unfused vs Fused)
#
# Compares CPU clock cycles (T-states) of evaluating both 32-bit division and
# modulo between unfused separate library calls (___udivsi3 + ___umodsi3) and
# fused single library call (___udivmodsi4).
#
# Usage:
#   ./z80-utils/scripts/benchmark-divmod-fusion.sh
#   BUILD_DIR=/path/to/build ./z80-utils/scripts/benchmark-divmod-fusion.sh

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
typedef unsigned long uint32_t;

__attribute__((noinline))
uint32_t fused_divmod(uint32_t a, uint32_t b) {
    uint32_t q = a / b;
    uint32_t r = a % b;
    return q + r;
}

__attribute__((noinline))
uint32_t separate_div(uint32_t a, uint32_t b) {
    return a / b;
}

__attribute__((noinline))
uint32_t separate_mod(uint32_t a, uint32_t b) {
    return a % b;
}

__attribute__((noinline))
uint32_t unfused_divmod(uint32_t a, uint32_t b) {
    uint32_t q = separate_div(a, b);
    uint32_t r = separate_mod(a, b);
    return q + r;
}

#ifdef TEST_FUSED
int main(void) {
    uint32_t sum = 0;
    for (uint32_t i = 1; i <= 50; i++) {
        sum += fused_divmod(i * 123456UL + 789UL, i * 37UL + 5UL);
    }
    return (int)(sum & 0xFFFF);
}
#else
int main(void) {
    uint32_t sum = 0;
    for (uint32_t i = 1; i <= 50; i++) {
        sum += unfused_divmod(i * 123456UL + 789UL, i * 37UL + 5UL);
    }
    return (int)(sum & 0xFFFF);
}
#endif
C_EOF

run_mode() {
    local mode="$1"
    local def=""
    if [ "$mode" = "fused" ]; then
        def="-DTEST_FUSED"
    fi
    "$CLANG" --target=z80 -O2 $def -c -nostdlib -ffreestanding "$BENCH_C" -o "$TMPDIR/test_$mode.o"
    "$LLD" --gc-sections -T "$LDSCRIPT" "$CRT0" "$TMPDIR/test_$mode.o" "$STAGE"/builtins/*.o -o "$TMPDIR/test_$mode.elf"
    "$OBJCOPY" -O binary "$TMPDIR/test_$mode.elf" "$TMPDIR/test_$mode.bin"
    local halt_addr
    halt_addr=$("$NM" "$TMPDIR/test_$mode.elf" | grep " _halt$" | awk '{print "0x"$1}')
    z88dk-ticks -end "$halt_addr" -counter 100000000 -output "$TMPDIR/test_$mode.ram" "$TMPDIR/test_$mode.bin" | tail -n 1
}

UNFUSED_CYCLES=$(run_mode unfused)
FUSED_CYCLES=$(run_mode fused)

SAVED=$((UNFUSED_CYCLES - FUSED_CYCLES))
PCT=$(awk "BEGIN {printf \"%.1f\", ($SAVED / $UNFUSED_CYCLES) * 100}")
SPEEDUP=$(awk "BEGIN {printf \"%.2f\", ($UNFUSED_CYCLES / $FUSED_CYCLES)}")

echo "### 32-Bit Division/Modulo Fusion Performance Measurement (Z80 T-States)"
echo ""
echo "| Mode | Runtime Libcalls | Clock Cycles (T-states) | Improvement vs Unfused |"
echo "|:---|:---|:---:|:---:|"
echo "| **Unfused** (separate calls) | \`___udivsi3\`, \`___umodsi3\` | $UNFUSED_CYCLES T-states | Baseline |"
echo "| **Fused** (PR #374) | \`___udivmodsi4\` | $FUSED_CYCLES T-states | **-${SAVED} T-states** (-${PCT}%) |"
echo ""
echo "*Measured on 50 iterations of paired 32-bit division and modulo via \`z88dk-ticks\`.*"
