; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 -z80-float-sdcccall0 %s -o - | FileCheck %s
; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 %s -o - | FileCheck --check-prefix=DEFAULT %s
;
; Under `-z80-float-sdcccall0`, the i32<->f32 conversion libcalls
; (__fixsfsi/__fixunssfsi/__floatsisf/__floatunsisf) are emitted with
; CallingConv::Z80_SDCCCall0 instead of the default CallingConv::C -- same
; ABI story as the arithmetic/compare libcalls in the sibling tests. These
; are carved out of the generic libcallForCartesianProduct path (which picks
; its CallingConv from a table this backend does not customize) via a
; .customFor({{S32, S32}}) legalization rule.
;
; C source (compiled with -mllvm -z80-float-sdcccall0 to target the
; sdcccall(0) runtime):
;   int to_int(float a) { return (int)a; }
;   float to_float(int a) { return (float)a; }
; One operand pushed (2x PUSH HL, one per 16-bit half) before the call,
; matching an sdcccall(0) runtime's stack-args ABI.

define i32 @to_int(float %a) {
; CHECK-LABEL: _to_int:
; CHECK: push hl
; CHECK: push hl
; CHECK: call ___fixsfsi
;
; DEFAULT-LABEL: _to_int:
; DEFAULT: call ___fixsfsi
  %r = fptosi float %a to i32
  ret i32 %r
}

define float @to_float(i32 %a) {
; CHECK-LABEL: _to_float:
; CHECK: push hl
; CHECK: push hl
; CHECK: call ___floatsisf
  %r = sitofp i32 %a to float
  ret float %r
}
