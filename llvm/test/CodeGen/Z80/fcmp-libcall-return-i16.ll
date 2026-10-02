; RUN: llc -mtriple=z80 -stop-after=legalizer -o - %s | FileCheck %s
;
; C source reproducer:
;
;   volatile double a = 3.0;
;   int test_eq(void) {
;       return a == a; // evaluated to 0 (false) on Z80 before fix
;   }
;
; Equivalent two-argument C function (avoids compile-time constant folding):
;
;   int deq(double a, double b) {
;       return a == b;
;   }
;
; Regression test: GlobalISel's createFCMPLibcall hardcoded the soft-float
; comparison libcall (__eqdf2, __nedf2, __ltdf2, ...) return type as i32 and
; built the following G_ICMP-with-#0 on i32. The GCC soft-float ABI specifies
; that these routines return a C `int`, which is 16-bit on Z80. Reading a 32-bit
; result from a routine that only defines the low 16 bits left the high word as
; callee garbage, so `a == a` could evaluate to false.
;
; The fix routes createFCMPLibcall through TargetLowering::getCmpLibcallReturnType
; (overridden to i16 for Z80), so the libcall result and the G_ICMP-with-#0 are
; both the target's C-int width.
;
; This test pins the width at the point the bug lived: the FCMP libcall result
; must be a 16-bit value and the compare-with-zero must be on s16 / i16, never
; s32 / i32.

target datalayout = "e-m:o-p:16:8-i16:8-i32:8-i64:8-i128:8-f32:8-f64:8-n8:16"
target triple = "z80"

; CHECK-LABEL: name: deq
; The comparison libcall result is copied out as a 16-bit value ...
; CHECK: {{%[0-9]+}}:_(s16) = COPY $de
; CHECK-NOT: COPY $hl
; ... the compare constant is i16 (NOT i32) ...
; CHECK: {{%[0-9]+}}:_(s16) = G_CONSTANT i16 0
; ... and the G_ICMP-with-#0 operates on s16 (NOT s32).
; CHECK: G_ICMP intpred(eq), {{%[0-9]+}}(s16), {{%[0-9]+}}
; CHECK-NOT: G_CONSTANT i32 0
define i16 @deq(double %a, double %b) {
  %c = fcmp oeq double %a, %b
  %z = zext i1 %c to i16
  ret i16 %z
}
