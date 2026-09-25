; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 -z80-float-sdcccall0 %s -o - | FileCheck %s
; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 %s -o - | FileCheck --check-prefix=DEFAULT %s
;
; Under `-z80-float-sdcccall0`, the f32 compare libcalls (__cmpsf2/__gtsf2/
; __gesf2/__unordsf2) are emitted with CallingConv::Z80_SDCCCall0 instead of
; the default CallingConv::C -- same ABI story as the arithmetic libcalls in
; issue-277-f32-libcall-sdcccall0.ll.
;
; C source (compiled with -mllvm -z80-float-sdcccall0 to target the
; sdcccall(0) runtime):
;   int lt(float a, float b) { return a < b; }
;   int gt(float a, float b) { return a > b; }
; Both operands pushed (4x PUSH HL) before the compare libcall, matching an
; sdcccall(0) runtime's stack-args ABI.

define i16 @lt(float %a, float %b) {
; CHECK-LABEL: _lt:
; CHECK: push hl
; CHECK: push hl
; CHECK: push hl
; CHECK: push hl
; CHECK: call ___cmpsf2
;
; DEFAULT-LABEL: _lt:
; DEFAULT: call ___cmpsf2
  %c = fcmp olt float %a, %b
  %r = zext i1 %c to i16
  ret i16 %r
}

define i16 @gt(float %a, float %b) {
; CHECK-LABEL: _gt:
; CHECK: push hl
; CHECK: push hl
; CHECK: push hl
; CHECK: push hl
; CHECK: call ___gtsf2
  %c = fcmp ogt float %a, %b
  %r = zext i1 %c to i16
  ret i16 %r
}
