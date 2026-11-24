; RUN: llc -verify-machineinstrs -mtriple=z80 -O2 < %s | FileCheck %s
;
; The byte reloaded into L after the first call is addressed through HL, so
; the reload saves HL around it. The call clobbers HL, so the PUSH reads a
; register with no value and has to mark the read undefined.

declare i16 @g(i8, i8)

define i16 @f(i8 %x) {
entry:
  %r0 = call i16 @g(i8 0, i8 0)
  %r1 = tail call i16 @g(i8 0, i8 %x)
  ret i16 %r1
}

; CHECK-LABEL: _f:
; CHECK:       call _g
; CHECK:       push hl
; CHECK:       ld a,(hl)
; CHECK-NEXT:  pop hl
; CHECK-NEXT:  ld l,a
; CHECK:       jp _g
