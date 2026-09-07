; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s
;
; Controls for __z88dk_callee (cc 132): the neighbouring conventions it must not
; disturb.

; ----------------------------------------------------------------------------
; __sdcccall(0) (cc 128): the caller pushes the args and the CALLER pops them
; after the call (`pop af` x2 for two i16 args).  That cleanup side is exactly
; what distinguishes it from __z88dk_callee, whose caller does not clean up.
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
