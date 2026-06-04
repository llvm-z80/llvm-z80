#!/usr/bin/env bash
# Enforce that the Z80 backend emits -verify-machineinstrs-clean code at the
# PRODUCTION opt levels (-Oz/-Os/-O2 with +static-stack) across the clang test
# corpus.  A "Using an undefined physical register" / "Bad machine code" abort
# is a latent stale-liveness miscompile (seen downstream as the ravn/llvm-z80 #136/#205/#197 class), so this gate
# keeps the production codegen surface verifier-clean.
#
# O0 is intentionally NOT swept: it has a broad, non-production O0-coarse-liveness
# residual (pre-PEI struct-copy/address borrows reading undef HL) tracked
# separately; production ships at -Oz/-Os, never O0.
#
# Usage: verify-production.sh <clang> <testcase-dir> [parallelism]
# Memory-bounded: parallelism defaults to 2 (~0.8 GB; each clang peaks ~0.4 GB),
# plus a per-process CPU cap so a runaway can't exhaust the runner.
set -uo pipefail

CLANG="${1:?clang path}"
TC="${2:?testcase dir}"
J="${3:-2}"

export CLANG
one() {
  local f="$1" opt="$2"
  ( ulimit -t 120 2>/dev/null
    "$CLANG" --target=z80 -nostdlib -ffreestanding "-$opt" \
      -Xclang -target-feature -Xclang +static-stack \
      -mllvm -verify-machineinstrs -c "$f" -o /dev/null ) 2>&1 \
    | grep -m1 -iE "Bad machine code|undefined physical register" \
    | sed "s|^|VERIFY-FAIL [-$opt +static-stack] $(basename "$f"): |"
}
export -f one

tmp="$(mktemp)"
for opt in Oz Os O2; do
  ls "$TC"/*.c | xargs -P "$J" -I{} bash -c 'one "$1" "$2"' _ {} "$opt"
done | tee "$tmp"

n=$(grep -c VERIFY-FAIL "$tmp" || true)
rm -f "$tmp"
echo "=== production-opt-level -verify-machineinstrs failures: $n ==="
[ "$n" -eq 0 ] || { echo "FAILED: production codegen is not verifier-clean"; exit 1; }
echo "OK: production codegen is -verify-machineinstrs clean"
