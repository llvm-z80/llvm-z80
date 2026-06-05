#!/bin/bash
# Z80/SM83 Compiler Benchmark: Clang vs SDCC
#
# Compares code size and execution speed (T-states) for identical C programs.
# Both compilers link against our crt0 for fair halt-based measurement.
#
# Usage: ./run_benchmark.sh [-target z80|sm83] [-O<level>] [bench_pattern]
#
# Examples:
#   ./run_benchmark.sh                    # run all Z80 benchmarks (O1 only)
#   ./run_benchmark.sh -target sm83       # run all SM83 benchmarks
#   ./run_benchmark.sh -O0               # run with O0 only
#   ./run_benchmark.sh -O1               # run with O1 only
#   ./run_benchmark.sh -O2               # run with O2 only
#   ./run_benchmark.sh -O3               # run with O3 only
#   ./run_benchmark.sh -Os               # run with Os only (optimize for size)
#   ./run_benchmark.sh -Oz               # run with Oz only (minimize size)
#   ./run_benchmark.sh -target sm83 sort  # run SM83 sort benchmark

# --- Parse args ---
TARGET="z80"
PATTERN=""
OPT_LEVEL=""
while [ $# -gt 0 ]; do
    case "$1" in
        -target) TARGET="$2"; shift 2 ;;
        -O0|-O1|-O2|-O3|-Os|-Oz) OPT_LEVEL="${1#-}"; shift ;;
        *) PATTERN="$1"; shift ;;
    esac
done
# Default: run O1 only
if [ -z "$OPT_LEVEL" ]; then
    OPT_LEVEL="O1"
fi

if [ "$TARGET" != "z80" ] && [ "$TARGET" != "sm83" ]; then
    echo "ERROR: invalid target '$TARGET' (use z80 or sm83)" >&2
    exit 1
fi

# --- Colors ---
if [ -t 1 ]; then
    C_HDR='\033[1;36m' C_OK='\033[32m' C_WARN='\033[33m'
    C_ERR='\033[31m' C_DIM='\033[2m' C_RST='\033[0m'
    C_BOLD='\033[1m'
else
    C_HDR='' C_OK='' C_WARN='' C_ERR='' C_DIM='' C_RST='' C_BOLD=''
fi

# --- Target-specific configuration ---
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$(cd "$SCRIPT_DIR/../../build" && pwd)}"
CLANG="$BUILD_DIR/bin/clang"

if [ "$TARGET" = "sm83" ]; then
    CLANG_TARGET="sm83"
    SDCC_ARCH="-msm83"
    ASSEMBLER="sdasgb"
    LINKER="sdldgb"
    EMU_FLAGS="-mgbz80"
    CRT0="$BUILD_DIR/lib/sm83/sm83_crt0.rel"
    SDCC_LIB_DIR="/usr/share/sdcc/lib/sm83"
    SDCC_LIB_NAME="sm83"
    RESULT_REG="bc"
else
    CLANG_TARGET="z80"
    SDCC_ARCH="-mz80"
    ASSEMBLER="sdasz80"
    LINKER="sdldz80"
    EMU_FLAGS=""
    CRT0="$BUILD_DIR/lib/z80/z80_crt0.rel"
    SDCC_LIB_DIR="/usr/share/sdcc/lib/z80"
    SDCC_LIB_NAME="z80"
    RESULT_REG="de"
fi

TMPDIR="${SCRIPT_DIR}/tmp_$$"
mkdir -p "$TMPDIR"
trap "rm -rf '$TMPDIR'" EXIT

HALT_ADDR="0x0006"
COMPILE_TIMEOUT=30
EMU_TIMEOUT=10

# --- Check tools ---
for tool in "$CLANG" sdcc "$ASSEMBLER" "$LINKER" makebin z88dk-ticks; do
    if ! command -v "$tool" &>/dev/null && [ ! -x "$tool" ]; then
        echo -e "${C_ERR}ERROR: $tool not found${C_RST}" >&2
        exit 1
    fi
done

if [ ! -f "$CRT0" ]; then
    echo -e "${C_ERR}ERROR: crt0 not found: $CRT0${C_RST}" >&2
    exit 1
fi

# --- Calculate code size from IHX file (sum of data record bytes) ---
ihx_code_size() {
    local ihx="$1"
    if [ ! -f "$ihx" ]; then echo "0"; return; fi
    awk -F '' '/^:/{
        ll = strtonum("0x" substr($0,2,2));
        tt = strtonum("0x" substr($0,8,2));
        if (tt == 0) total += ll;
    } END { print total+0 }' "$ihx"
}

# --- Compile with Clang ---
compile_clang() {
    local src="$1" opt="$2" tag="$3"
    local ihx="$TMPDIR/${tag}.ihx"
    local bin="$TMPDIR/${tag}.bin"

    timeout "$COMPILE_TIMEOUT" "$CLANG" --target="$CLANG_TARGET" -"$opt" \
        "$src" -o "$ihx" >"$TMPDIR/${tag}_err.txt" 2>&1
    if [ $? -ne 0 ]; then
        echo "COMPILE_ERROR"
        return 1
    fi
    makebin "$ihx" "$bin" 2>/dev/null
    echo "$bin"
}

# --- Compile with SDCC ---
compile_sdcc() {
    local src="$1" opt_flag="$2" tag="$3"
    local asm_file="$TMPDIR/${tag}.asm"
    local rel_file="$TMPDIR/${tag}.rel"
    local base="$TMPDIR/${tag}"
    local ihx="${base}.ihx"
    local bin="${base}.bin"

    # SDCC compile to asm
    timeout "$COMPILE_TIMEOUT" sdcc $SDCC_ARCH --std-c11 $opt_flag \
        -S "$src" -o "$asm_file" >"$TMPDIR/${tag}_err.txt" 2>&1
    if [ $? -ne 0 ]; then
        echo "COMPILE_ERROR"
        return 1
    fi

    # Assemble
    "$ASSEMBLER" -g -o "$rel_file" "$asm_file" 2>>"$TMPDIR/${tag}_err.txt"
    if [ $? -ne 0 ]; then
        echo "ASM_ERROR"
        return 1
    fi

    # Link with our crt0 + SDCC's library for runtime functions
    local link_args=("$CRT0" "$rel_file")
    if [ -d "$SDCC_LIB_DIR" ]; then
        link_args+=("-k" "$SDCC_LIB_DIR" "-l" "$SDCC_LIB_NAME")
    fi
    "$LINKER" -i "$base" "${link_args[@]}" >>"$TMPDIR/${tag}_err.txt" 2>&1
    if [ $? -ne 0 ]; then
        echo "LINK_ERROR"
        return 1
    fi

    makebin "$ihx" "$bin" 2>/dev/null
    echo "$bin"
}

# --- Measure binary ---
measure() {
    local bin="$1" ihx="$2"
    if [ ! -f "$bin" ]; then
        echo "0 0 ERROR"
        return 1
    fi

    local size=$(ihx_code_size "$ihx")

    # T-states
    local tstates=$(timeout "$EMU_TIMEOUT" z88dk-ticks $EMU_FLAGS -end "$HALT_ADDR" "$bin" 2>&1 | tail -1)

    # Correctness: check result register
    local reg_val=$(timeout "$EMU_TIMEOUT" z88dk-ticks $EMU_FLAGS -trace -end "$HALT_ADDR" "$bin" 2>&1 \
        | grep -oi "${RESULT_REG}=[0-9A-F]*" | tail -1 | sed "s/${RESULT_REG}=//i")

    echo "$size $tstates $reg_val"
}

# --- Print table header ---
print_header() {
    echo ""
    printf "  ${C_BOLD}%-22s  %-3s  %7s %7s  %8s %8s  %s${C_RST}\n" \
        "Benchmark" "Opt" "Clang" "SDCC" "Clang" "SDCC" "Winner"
    printf "  ${C_BOLD}%-22s  %-3s  %7s %7s  %8s %8s  %s${C_RST}\n" \
        "" "" "(bytes)" "(bytes)" "(T-cyc)" "(T-cyc)" ""
    printf "  %-22s  %-3s  %7s %7s  %8s %8s  %s\n" \
        "$(printf '%0.s-' {1..22})" "---" \
        "-------" "-------" "--------" "--------" "----------"
}

# --- Compare function ---
compare_winner() {
    local clang_val="$1" sdcc_val="$2"
    if [ "$clang_val" -lt "$sdcc_val" ] 2>/dev/null; then
        echo "Clang"
    elif [ "$clang_val" -gt "$sdcc_val" ] 2>/dev/null; then
        echo "SDCC"
    else
        echo "Tie"
    fi
}

# --- Main ---
TARGET_UPPER=$(echo "$TARGET" | tr '[:lower:]' '[:upper:]')
echo -e "${C_HDR}${TARGET_UPPER} Compiler Benchmark: Clang vs SDCC${C_RST}"
echo "======================================"
echo "Target:   $TARGET"
echo "Build:    $BUILD_DIR"
echo "SDCC:     $(sdcc --version 2>&1 | head -1 | sed 's/SDCC : //')"
echo "crt0:     shared (halt @ $HALT_ADDR)"
echo "Result:   ${RESULT_REG^^} register (expected per benchmark)"
echo "SDCC opt: Os/Oz → --opt-code-size, O2/O3 → --opt-code-speed, O0/O1 → (default)"

print_header

TOTAL=0
CLANG_SIZE_WINS=0; SDCC_SIZE_WINS=0
CLANG_SPEED_WINS=0; SDCC_SPEED_WINS=0
ERRORS=0

for bench_file in "$SCRIPT_DIR"/bench_*.c; do
    name="$(basename "$bench_file" .c)"
    if [ -n "$PATTERN" ] && [[ "$name" != *"$PATTERN"* ]]; then
        continue
    fi

    # Parse expected value from source: "expect 0xNNNN" comment
    RESULT_EXPECT=$(grep -oP 'expect\s+0x\K[0-9A-Fa-f]+' "$bench_file" | head -1)
    RESULT_EXPECT=$(echo "$RESULT_EXPECT" | tr '[:lower:]' '[:upper:]')
    if [ -z "$RESULT_EXPECT" ]; then RESULT_EXPECT="000F"; fi

    for opt in $OPT_LEVEL; do
        TOTAL=$((TOTAL + 1))

        # SDCC optimization flags
        # Os/Oz → --opt-code-size, O2/O3 → --opt-code-speed, O0/O1 → default
        local_sdcc_opt=""
        case "$opt" in
            Os|Oz) local_sdcc_opt="--opt-code-size" ;;
            O2|O3) local_sdcc_opt="--opt-code-speed" ;;
            *)     local_sdcc_opt="" ;;
        esac

        tag_clang="${name}_clang_${opt}"
        tag_sdcc="${name}_sdcc_${opt}"

        # Build both
        clang_bin=$(compile_clang "$bench_file" "$opt" "$tag_clang")
        sdcc_bin=$(compile_sdcc "$bench_file" "$local_sdcc_opt" "$tag_sdcc")

        # Handle errors
        if [[ "$clang_bin" == *ERROR* ]] || [[ "$sdcc_bin" == *ERROR* ]]; then
            err_detail=""
            [[ "$clang_bin" == *ERROR* ]] && err_detail="Clang: $clang_bin"
            [[ "$sdcc_bin" == *ERROR* ]] && err_detail="${err_detail:+$err_detail, }SDCC: $sdcc_bin"
            printf "  ${C_ERR}%-22s  %-3s  %-60s${C_RST}\n" "$name" "$opt" "ERROR ($err_detail)"
            ERRORS=$((ERRORS + 1))
            continue
        fi

        # IHX paths for code size measurement
        clang_ihx="$TMPDIR/${tag_clang}.ihx"
        sdcc_ihx="$TMPDIR/${tag_sdcc}.ihx"

        # Measure both
        read clang_size clang_tstates clang_reg <<< $(measure "$clang_bin" "$clang_ihx")
        read sdcc_size sdcc_tstates sdcc_reg <<< $(measure "$sdcc_bin" "$sdcc_ihx")

        # Check correctness
        clang_ok=""; sdcc_ok=""
        if [ "$(echo "$clang_reg" | tr '[:lower:]' '[:upper:]')" != "$RESULT_EXPECT" ]; then
            clang_ok=" ${C_ERR}!${C_RST}"
        fi
        if [ "$(echo "$sdcc_reg" | tr '[:lower:]' '[:upper:]')" != "$RESULT_EXPECT" ]; then
            sdcc_ok=" ${C_ERR}!${C_RST}"
        fi

        # Determine winners
        size_winner=$(compare_winner "$clang_size" "$sdcc_size")
        speed_winner=$(compare_winner "$clang_tstates" "$sdcc_tstates")

        [ "$size_winner" = "Clang" ] && CLANG_SIZE_WINS=$((CLANG_SIZE_WINS + 1))
        [ "$size_winner" = "SDCC" ] && SDCC_SIZE_WINS=$((SDCC_SIZE_WINS + 1))
        [ "$speed_winner" = "Clang" ] && CLANG_SPEED_WINS=$((CLANG_SPEED_WINS + 1))
        [ "$speed_winner" = "SDCC" ] && SDCC_SPEED_WINS=$((SDCC_SPEED_WINS + 1))

        # Format winner column
        if [ "$size_winner" = "$speed_winner" ]; then
            case "$size_winner" in
                Clang) winner_str="${C_OK}Clang${C_RST}" ;;
                SDCC)  winner_str="${C_WARN}SDCC${C_RST}" ;;
                *)     winner_str="Tie" ;;
            esac
        else
            winner_str="Size:${size_winner} Spd:${speed_winner}"
        fi

        printf "  %-22s  %-3s  %5s B %5s B  %7s T %7s T  %b%b%b\n" \
            "$name" "$opt" \
            "$clang_size" "$sdcc_size" \
            "$clang_tstates" "$sdcc_tstates" \
            "$winner_str" "$clang_ok" "$sdcc_ok"
    done
done

# --- Summary ---
SIZE_TIE=$((TOTAL - ERRORS - CLANG_SIZE_WINS - SDCC_SIZE_WINS))
SPEED_TIE=$((TOTAL - ERRORS - CLANG_SPEED_WINS - SDCC_SPEED_WINS))
echo ""
echo "======================================"
echo "Total: $TOTAL comparisons ($ERRORS errors)"
echo ""
echo "Code size wins:  Clang=$CLANG_SIZE_WINS  SDCC=$SDCC_SIZE_WINS  Tie=$SIZE_TIE"
echo "Speed wins:      Clang=$CLANG_SPEED_WINS  SDCC=$SDCC_SPEED_WINS  Tie=$SPEED_TIE"
