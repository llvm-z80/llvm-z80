// RUN: %clang_cc1 -triple z80 -Oz -S -o - %s | FileCheck %s
// XFAIL: *
//
// DEMONSTRATION TEST (ravn/llvm-z80#168) — expected to FAIL on current upstream;
// the CHECK lines assert the POST-FIX behavior.  Remove the `XFAIL` line once
// the SimplifyCFG cost gate lands.
//
// SimplifyCFG's foldTwoEntryPHINode speculatively converts an if/else diamond
// into a `select`, computing BOTH arms unconditionally.  On a target with no
// branch predictor (getPredictableBranchThreshold().isZero(), e.g. Z80) that is
// a net loss whenever the speculated arm has any non-free cost: a conditional
// branch would skip the work half the time.  The fix cost-gates the fold (skip
// when Cost > TCC_Free on such targets), so this conditional XOR keeps its
// branch instead of becoming a branchless compute-both.
//
// Expected (fixed) Z80 codegen: the `xor 27` is GUARDED by a conditional jump,
// i.e. executed only on the taken path.  Without the fix it is hoisted and
// computed unconditionally (the branchless compute-both pattern), so the
// guarding `jr z`/`jr nz` is absent.

// CHECK-LABEL: _cond_xor:
// CHECK:       jr {{n?z}},
// CHECK:       xor 27
unsigned char cond_xor(unsigned char x, int c) {
    return c ? (x ^ 27) : x;
}
