; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s

; Test: Z80_AllReg calling convention (cc 129) passes all args in registers.

; 1 x i16 arg → HL
define cc 129 void @one_i16(i16 %a) {
; CHECK-LABEL: _one_i16:
; CHECK-NOT:   ix
; CHECK:       ret
  ret void
}

; 2 x i16 args → HL, DE; subtraction makes the assignment order observable.
; CHECK-LABEL: _sub_two_i16:
; CHECK:       and a
; CHECK:       sbc hl,de
define cc 129 i16 @sub_two_i16(i16 %a, i16 %b) {
  %r = sub i16 %a, %b
  ret i16 %r
}

; 3 x i16 args → HL, DE, BC
define cc 129 i16 @three_i16(i16 %a, i16 %b, i16 %c) {
; CHECK-LABEL: _three_i16:
; CHECK-NOT:   ld{{.*}}(ix
; CHECK:       ret
  %ab = sub i16 %a, %b
  %r = sub i16 %ab, %c
  ret i16 %r
}

; 1 x i8 arg → A
define cc 129 i8 @one_i8(i8 %a) {
; CHECK-LABEL: _one_i8:
; CHECK:       ret
  ret i8 %a
}

; i8 + i16 → A, HL
define cc 129 i16 @mixed_i8_i16(i8 %a, i16 %b) {
; CHECK-LABEL: _mixed_i8_i16:
; CHECK:       ret
  ret i16 %b
}

; 5 x i16 args → HL, DE, BC, IX, IY (all register pairs used)
define cc 129 i16 @five_i16(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e) {
; CHECK-LABEL: _five_i16:
; CHECK-NOT:   ld{{.*}}(ix+
; CHECK:       ret
  %ab = sub i16 %a, %b
  %abc = sub i16 %ab, %c
  %abcd = sub i16 %abc, %d
  %r = sub i16 %abcd, %e
  ret i16 %r
}
