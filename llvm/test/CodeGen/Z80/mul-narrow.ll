; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s | FileCheck %s

; A multiply runs at 16 or 8 bits when only its low part is used, or when its
; operands are small enough for the product to fit in 16 bits.

; CHECK-LABEL: _mul_low_half:
; CHECK-NOT:   ___mulsi3
; CHECK:       call ___mulhi3
define i16 @mul_low_half(i32 %a, i32 %b) {
  %m = mul i32 %a, %b
  %t = trunc i32 %m to i16
  ret i16 %t
}

; CHECK-LABEL: _mul_low_byte:
; CHECK-NOT:   call
; CHECK:       ret
define i8 @mul_low_byte(i16 %a, i16 %b) {
  %m = mul i16 %a, %b
  %t = trunc i16 %m to i8
  ret i8 %t
}

; 4 and 7 bits make an 11-bit product.
; CHECK-LABEL: _small_mul:
; CHECK-NOT:   ___mulsi3
; CHECK:       call ___mulhi3
define i32 @small_mul(i32 %a, i32 %b) {
  %x = and i32 %a, 15
  %y = and i32 %b, 127
  %m = mul i32 %x, %y
  ret i32 %m
}

; 17 and 3 bits make a 20-bit product.
; CHECK-LABEL: _wide_mul:
; CHECK:       call ___mulsi3
define i32 @wide_mul(i32 %a, i32 %b) {
  %x = and i32 %a, 131071
  %y = and i32 %b, 7
  %m = mul i32 %x, %y
  ret i32 %m
}
