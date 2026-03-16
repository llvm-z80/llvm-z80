; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s

; Test: 32-bit addition (HLDE + stack args)
; Uses add hl,de for low word and adc hl,de for high word
define i32 @add32(i32 %a, i32 %b) {
; CHECK-LABEL: add32:
; CHECK:       add hl,de
; CHECK:       adc hl,bc
  %r = add i32 %a, %b
  ret i32 %r
}

; Test: 32-bit subtraction
; Uses sbc hl,de for both words with carry propagation
define i32 @sub32(i32 %a, i32 %b) {
; CHECK-LABEL: sub32:
; CHECK:       sbc hl,
; CHECK:       sbc hl,
  %r = sub i32 %a, %b
  ret i32 %r
}
