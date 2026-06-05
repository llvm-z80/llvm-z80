; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s

; Test: i64 return uses sret (return value stored via pointer in HL)
define i64 @identity64(i64 %x) {
; CHECK-LABEL: _identity64:
; CHECK:       ret
  ret i64 %x
}

; Test: i64 addition (narrowed to 4 x i16 add/adc chain)
define i64 @add64(i64 %a, i64 %b) {
; CHECK-LABEL: _add64:
; CHECK:       adc hl,
; CHECK:       adc hl,
; CHECK:       adc hl,
; CHECK:       ret
  %r = add i64 %a, %b
  ret i64 %r
}

; Test: i64 subtraction
define i64 @sub64(i64 %a, i64 %b) {
; CHECK-LABEL: _sub64:
; CHECK:       ret
  %r = sub i64 %a, %b
  ret i64 %r
}

; Test: calling a function that returns i64 (caller-side sret)
define i16 @call_add64(i64 %a, i64 %b) {
; CHECK-LABEL: _call_add64:
; CHECK:       call _add64
; CHECK:       ret
  %r = call i64 @add64(i64 %a, i64 %b)
  %lo = trunc i64 %r to i16
  ret i16 %lo
}
