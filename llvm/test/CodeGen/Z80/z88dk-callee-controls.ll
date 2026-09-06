; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s
;
; Positive controls for the __z88dk_callee work (planned cc 131).  These pin the
; neighbouring __smallc / sdcccall(0) convention (cc 128), which shares the
; stack-passing of __z88dk_callee but keeps CALLER-side cleanup.  Implementing
; cc 131 must not turn cc 128 into a callee-cleanup convention.  NOT XFAIL: pass
; today and must keep passing.

; ----------------------------------------------------------------------------
; __smallc (cc 128): caller pushes args, and the CALLER pops them after the
; call (`pop af` x2 for two i16 args).  This is exactly what distinguishes it
; from __z88dk_callee, whose caller does NOT clean up.
; ----------------------------------------------------------------------------
declare cc 128 void @sc2(i16, i16)
define void @sc_caller() {
; CHECK-LABEL: _sc_caller:
; CHECK:      push hl
; CHECK:      call _sc2
; CHECK:      pop af
; CHECK:      pop af
  call cc 128 void @sc2(i16 1, i16 2)
  ret void
}

; ----------------------------------------------------------------------------
; Plain default C convention is unaffected: register args, no stack cleanup.
; ----------------------------------------------------------------------------
declare void @plain2(i16, i16)
define void @plain_caller() {
; CHECK-LABEL: _plain_caller:
; CHECK-NOT:  push hl
; CHECK-NOT:  pop af
  call void @plain2(i16 1, i16 2)
  ret void
}
