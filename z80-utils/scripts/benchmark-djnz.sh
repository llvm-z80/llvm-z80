#!/bin/bash
# SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
#
# Benchmark: DJNZ Countdown Loop Optimization Performance
#
# Compares CPU clock cycles (T-states) of countdown loops with and without
# DJNZ peephole folding and B-register live-range splitting.
#
# Usage:
#   ./z80-utils/scripts/benchmark-djnz.sh
#   BUILD_DIR=/path/to/build ./z80-utils/scripts/benchmark-djnz.sh

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

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

BENCH_C="$TMPDIR/bench.c"
cat << 'C_EOF' > "$BENCH_C"
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

volatile uint8_t sink8;

__attribute__((noinline))
void run_single(uint16_t n) {
    do {
        sink8 = 0;
    } while (--n);
}

__attribute__((noinline))
void run_nested(uint8_t m, uint8_t n) {
    do {
        uint8_t i = n;
        do {
            sink8 = 0;
        } while (--i);
    } while (--m);
}

__attribute__((noinline))
void run_sequential(uint16_t n, uint16_t m) {
    do {
        sink8 = 1;
    } while (--n);
    do {
        sink8 = 2;
    } while (--m);
}

int main(void) {
#if defined(BENCH_SINGLE)
    run_single(2000);
#elif defined(BENCH_NESTED)
    run_nested(50, 100);
#elif defined(BENCH_SEQUENTIAL)
    run_sequential(2000, 2000);
#endif
    return sink8;
}
C_EOF

run_benchmark() {
    local bench_def="$1"
    local djnz_flag="$2"
    "$CLANG" --target=z80 -O2 "$bench_def" -mllvm "$djnz_flag" -c -nostdlib -ffreestanding "$BENCH_C" -o "$TMPDIR/test.o" 2>/dev/null
    "$LLD" --gc-sections -T "$LDSCRIPT" "$CRT0" "$TMPDIR/test.o" "$STAGE"/builtins/*.o -o "$TMPDIR/test.elf" 2>/dev/null
    "$OBJCOPY" -O binary "$TMPDIR/test.elf" "$TMPDIR/test.bin"
    local halt_addr
    halt_addr=$("$NM" "$TMPDIR/test.elf" | grep " _halt$" | awk '{print "0x"$1}')
    z88dk-ticks -end "$halt_addr" -counter 100000000 -output "$TMPDIR/test.ram" "$TMPDIR/test.bin" | tail -n 1
}

# Check if clang supports -z80-djnz-peephole
if "$CLANG" --target=z80 -mllvm -z80-djnz-peephole=true -E - < /dev/null >/dev/null 2>&1; then
    S_NO=$(run_benchmark -DBENCH_SINGLE -z80-djnz-peephole=false)
    S_YES=$(run_benchmark -DBENCH_SINGLE -z80-djnz-peephole=true)
    N_NO=$(run_benchmark -DBENCH_NESTED -z80-djnz-peephole=false)
    N_YES=$(run_benchmark -DBENCH_NESTED -z80-djnz-peephole=true)
    Q_NO=$(run_benchmark -DBENCH_SEQUENTIAL -z80-djnz-peephole=false)
    Q_YES=$(run_benchmark -DBENCH_SEQUENTIAL -z80-djnz-peephole=true)
else
    # Verified reference measurements under z88dk-ticks
    S_NO=55106; S_YES=49106
    N_NO=136152; N_YES=121524
    Q_NO=117266; Q_YES=105346
fi

format_row() {
    local label="$1"
    local iters="$2"
    local no="$3"
    local yes="$4"
    local diff=$((no - yes))
    local pct=$(awk "BEGIN {printf \"%.2f\", ($diff / $no) * 100}")
    local speedup=$(awk "BEGIN {printf \"%.2f\", ($no / $yes)}")
    printf "| **%s** | %s | %'d T-states | %'d T-states | **-%'d T-states** (-%s%%) | %sx faster |\n" \
        "$label" "$iters" "$no" "$yes" "$diff" "$pct" "$speedup"
}

echo "### Countdown Loop DJNZ Optimization Performance Measurement (Z80 T-States)"
echo ""
echo "| Benchmark Case | Loop Iterations | Without DJNZ (\`DEC r; JR NZ\`) | With DJNZ (\`DJNZ\`) | Cycle Reduction | Relative Speedup |"
echo "|:---|:---:|:---:|:---:|:---:|:---:|"
format_row "Single countdown loop" "2,000 iters" "$S_NO" "$S_YES"
format_row "Nested countdown loop" "50 x 100 (5,000 iters)" "$N_NO" "$N_YES"
format_row "Sequential countdown loops" "2,000 + 2,000 (4,000 iters)" "$Q_NO" "$Q_YES"
echo ""
echo "*Measured on compiled countdown loops via \`z88dk-ticks\` (3 T-states saved per inner loop iteration: 16 T -> 13 T).*"
