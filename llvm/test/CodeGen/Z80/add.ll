; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s

define i8 @add8_reg_reg(i8 %a, i8 %b) {
; CHECK-LABEL: add8_reg_reg:
; CHECK:       add a,l
  %c = add i8 %a, %b
  ret i8 %c
}

define i16 @add16_reg_reg(i16 %a, i16 %b) {
; CHECK-LABEL: add16_reg_reg:
; CHECK:       add hl,de
; CHECK-NEXT:  ex de,hl
; CHECK-NEXT:  ret
  %c = add i16 %a, %b
  ret i16 %c
}

define i32 @add32_reg_reg(i32 %a, i32 %b) {
; CHECK-LABEL: add32_reg_reg:
; CHECK:       add hl,de
; CHECK:       adc hl,bc
  %c = add i32 %a, %b
  ret i32 %c
}
