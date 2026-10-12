; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s | FileCheck %s

; A 16-bit signed division or remainder of operands that fit in a signed byte
; runs the 8-bit loop when its result fits a byte.

; -128 / -1 is 128, which no byte holds, so a quotient used at 16 bits stays a
; 16-bit division.
; CHECK-LABEL: _div16:
; CHECK:       call ___divhi3
define i16 @div16(i8 %a, i8 %b) {
  %x = sext i8 %a to i16
  %y = sext i8 %b to i16
  %r = sdiv i16 %x, %y
  ret i16 %r
}

; CHECK-LABEL: _div8:
; CHECK-NOT:   call
; CHECK:       ld b,8
; CHECK-NOT:   call
; CHECK:       ret
define i8 @div8(i8 %a, i8 %b) {
  %x = sext i8 %a to i16
  %y = sext i8 %b to i16
  %r = sdiv i16 %x, %y
  %t = trunc i16 %r to i8
  ret i8 %t
}

; A mask keeps only the low byte as well.
; CHECK-LABEL: _div8_mask:
; CHECK-NOT:   call
; CHECK:       ld b,8
; CHECK-NOT:   call
; CHECK:       ret
define i16 @div8_mask(i8 %a, i8 %b) {
  %x = sext i8 %a to i16
  %y = sext i8 %b to i16
  %r = sdiv i16 %x, %y
  %m = and i16 %r, 255
  ret i16 %m
}

; -128 / -1 leaves 0x80, so the mask stays even for operands known negative.
; CHECK-LABEL: _div8_negative:
; CHECK:       ld b,8
; CHECK:       res 7,
define i8 @div8_negative(i8 %a, i8 %b) {
  %na = or i8 %a, -128
  %nb = or i8 %b, -128
  %x = sext i8 %na to i16
  %y = sext i8 %nb to i16
  %r = sdiv i16 %x, %y
  %t = trunc i16 %r to i8
  %m = and i8 %t, 127
  ret i8 %m
}

; A remainder is smaller than the divisor in magnitude.
; CHECK-LABEL: _rem16:
; CHECK-NOT:   call
; CHECK:       ld b,8
; CHECK-NOT:   call
; CHECK:       ret
define i16 @rem16(i8 %a, i8 %b) {
  %x = sext i8 %a to i16
  %y = sext i8 %b to i16
  %r = srem i16 %x, %y
  ret i16 %r
}

; Operands need not be sign extensions, only fit in a signed byte.
; CHECK-LABEL: _div8_ashr:
; CHECK-NOT:   call
; CHECK:       ld b,8
; CHECK-NOT:   call
; CHECK:       ret
define i8 @div8_ashr(i16 %a, i16 %b) {
  %x = ashr i16 %a, 8
  %y = ashr i16 %b, 8
  %r = sdiv i16 %x, %y
  %t = trunc i16 %r to i8
  ret i8 %t
}

; A power-of-two divisor is left to shifts and masks.
; CHECK-LABEL: _rem_pow2:
; CHECK-NOT:   call
; CHECK-NOT:   ld b,8
; CHECK:       ret
define i16 @rem_pow2(i8 %a) {
  %x = sext i8 %a to i16
  %r = srem i16 %x, 4
  ret i16 %r
}

; A remainder taken with its quotient stays a single __divmodhi4 call,
; whichever comes first.
; CHECK-LABEL: _rem_then_div:
; CHECK-NOT:   ld b,8
; CHECK:       call ___divmodhi4
; CHECK-NOT:   ld b,8
; CHECK:       ret
define i16 @rem_then_div(i8 %a, i8 %b) {
  %x = sext i8 %a to i16
  %y = sext i8 %b to i16
  %r = srem i16 %x, %y
  %d = sdiv i16 %x, %y
  %s = add i16 %r, %d
  ret i16 %s
}
