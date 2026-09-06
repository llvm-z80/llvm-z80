; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=z80 -O1 -stop-after=postrapseudos < %s | FileCheck %s --check-prefix=Z80MIR
; RUN: llc -mtriple=sm83 -O1 -stop-after=postrapseudos < %s | FileCheck %s --check-prefix=SM83MIR

; A float-returning function whose first argument is float is
; callee-cleanup (the SDCC float exception), and its return value
; travels in HLDE. The cleanup scratch must not be HL.

; CHECK-LABEL: pick:
; CHECK: ld iy,12
; CHECK-NEXT: add iy,sp
; CHECK-NEXT: ld sp,iy
; CHECK: ret

; The expansion has to keep the return registers on the instruction that
; leaves the function.
; Z80MIR-LABEL: name: pick
; Z80MIR: RET{{.*}}implicit $de{{.*}}implicit $hl
; SM83MIR-LABEL: name: pick
; SM83MIR: JP_HLind{{.*}}implicit $bc{{.*}}implicit $de
define float @pick(float %x, float %y, float %a, float %b) {
  ret float %b
}

; A 16-bit return keeps the cheaper HL scratch.
; CHECK-LABEL: sum:
; CHECK: ld hl,12
; CHECK: add hl,sp
; CHECK: ld sp,hl
; CHECK: ret

; Z80MIR-LABEL: name: sum
; Z80MIR: RET{{.*}}implicit $de
; SM83MIR-LABEL: name: sum
; SM83MIR: JP_HLind{{.*}}implicit $bc
define i16 @sum(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e, i16 %f, i16 %g, i16 %h) {
  %s = add i16 %g, %h
  ret i16 %s
}
