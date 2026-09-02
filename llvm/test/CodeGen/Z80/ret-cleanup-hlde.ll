; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s

; A float-returning function whose first argument is float is
; callee-cleanup (the SDCC float exception), and its return value
; travels in HLDE. The cleanup scratch must not be HL.

; CHECK-LABEL: pick:
; CHECK: ld iy,12
; CHECK-NEXT: add iy,sp
; CHECK-NEXT: ld sp,iy
; CHECK: ret
define float @pick(float %x, float %y, float %a, float %b) {
  ret float %b
}

; A 16-bit return keeps the cheaper HL scratch.
; CHECK-LABEL: sum:
; CHECK: ld hl,12
; CHECK: add hl,sp
; CHECK: ld sp,hl
; CHECK: ret
define i16 @sum(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e, i16 %f, i16 %g, i16 %h) {
  %s = add i16 %g, %h
  ret i16 %s
}
