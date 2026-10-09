; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s | FileCheck %s

; A 16-bit signed division or remainder of two sign-extended bytes runs the
; 8-bit loop when its result fits a byte.

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
