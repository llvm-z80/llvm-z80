; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s

; Test SHL+ASHR pattern matching for sign extension optimization.
; SHL 8 + ASHR 8 on i16 should use SEXT_GR8_GR16 pseudo (5 instructions)
; instead of naive 8x ADD + 8x SRA/RR (24 instructions).

define i16 @sext_inreg_8(i16 %x) {
; CHECK-LABEL: _sext_inreg_8:
; CHECK: ld a,l
; CHECK-NEXT: ld e,a
; CHECK-NEXT: rlca
; CHECK-NEXT: sbc a,a
; CHECK-NEXT: ld d,a
; CHECK-NEXT: ret
  %trunc = trunc i16 %x to i8
  %sext = sext i8 %trunc to i16
  ret i16 %sext
}
