; RUN: opt < %s -passes=aggressive-instcombine -S | FileCheck %s
; XFAIL: *
;
; DEMONSTRATION TEST (ravn/llvm-z80#158) — expected to FAIL on current upstream;
; the CHECK lines assert the POST-FIX behavior.  Remove the `XFAIL` line once
; the TruncInstCombine fix lands.
;
; TruncInstCombine narrows `trunc(iN expression-of-smaller-value)` graphs back
; to the natural width.  The walk bails at the first non-Instruction
; non-Constant operand, so it gives up whenever the expression reaches a
; function Argument.  The common shape is a K&R-style u8 parameter that the C
; ABI int-promotes to i16 on a 16-bit-int target (e.g. Z80): the body masks the
; argument back to 8 bits and the whole rotate expression is i16, gated behind a
; final `trunc i16 ... to i8`.  Without narrowing through the argument the
; target emits a 16-bit shift/mask/or dance instead of native 8-bit ops.  The
; fix accepts an Argument as a narrowable leaf and emits a single explicit trunc
; of the argument at function entry (dominating every narrowed use).  Pre-fix
; this function is left entirely at i16; the i8 ops below are the signal.

define zeroext i8 @rotl_u8(i16 noundef %0) {
; CHECK-LABEL: define zeroext i8 @rotl_u8(
; CHECK-SAME: i16 noundef [[TMP0:%.*]]) {
; CHECK-NEXT:    [[TMP2:%.*]] = trunc i16 [[TMP0]] to i8
; CHECK-NEXT:    [[TMP3:%.*]] = and i8 [[TMP2]], -1
; CHECK-NEXT:    [[TMP4:%.*]] = shl i8 [[TMP3]], 1
; CHECK-NEXT:    [[TMP5:%.*]] = lshr i8 [[TMP3]], 7
; CHECK-NEXT:    [[TMP6:%.*]] = or i8 [[TMP4]], [[TMP5]]
; CHECK-NEXT:    ret i8 [[TMP6]]
;
  %2 = and i16 %0, 255
  %3 = shl nuw nsw i16 %2, 1
  %4 = lshr i16 %2, 7
  %5 = or disjoint i16 %3, %4
  %6 = trunc i16 %5 to i8
  ret i8 %6
}
