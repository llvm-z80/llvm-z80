; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s | FileCheck %s

; A 32-bit multiply of operands that fit in 16 bits is one call that returns
; the whole product.

; CHECK-LABEL: _smul:
; CHECK:       call ___mulhisi3
define i32 @smul(i16 %a, i16 %b) {
  %x = sext i16 %a to i32
  %y = sext i16 %b to i32
  %m = mul nsw i32 %x, %y
  ret i32 %m
}

; CHECK-LABEL: _umul:
; CHECK:       call ___umulhisi3
define i32 @umul(i16 %a, i16 %b) {
  %x = zext i16 %a to i32
  %y = zext i16 %b to i32
  %m = mul nuw i32 %x, %y
  ret i32 %m
}

; CHECK-LABEL: _smul_const:
; CHECK:       call ___mulhisi3
define i32 @smul_const(i16 %a) {
  %x = sext i16 %a to i32
  %m = mul nsw i32 %x, -300
  ret i32 %m
}

; A zero-extended byte also fits in a signed 16-bit operand.
; CHECK-LABEL: _byte_by_half:
; CHECK:       call ___mulhisi3
define i32 @byte_by_half(i8 %a, i16 %b) {
  %x = zext i8 %a to i32
  %y = sext i16 %b to i32
  %m = mul i32 %x, %y
  ret i32 %m
}

; No 16-bit operand type holds both a signed and an unsigned 16-bit value.
; CHECK-LABEL: _mixed:
; CHECK:       call ___mulsi3
define i32 @mixed(i16 %a, i16 %b) {
  %x = sext i16 %a to i32
  %y = zext i16 %b to i32
  %m = mul i32 %x, %y
  ret i32 %m
}
