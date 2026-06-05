; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s

define i8 @sub8_reg_reg(i8 %a, i8 %b) {
; CHECK-LABEL: _sub8_reg_reg:
; CHECK:       sub l
  %c = sub i8 %a, %b
  ret i8 %c
}

define i16 @sub16_reg_reg(i16 %a, i16 %b) {
; CHECK-LABEL: _sub16_reg_reg:
; CHECK:       and a
; CHECK-NEXT:  sbc hl,de
; CHECK-NEXT:  ex de,hl
; CHECK-NEXT:  ret
  %c = sub i16 %a, %b
  ret i16 %c
}
