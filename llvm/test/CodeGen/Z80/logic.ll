; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s

define i8 @and8(i8 %a, i8 %b) {
; CHECK-LABEL: _and8:
; CHECK:       and l
  %c = and i8 %a, %b
  ret i8 %c
}

define i8 @or8(i8 %a, i8 %b) {
; CHECK-LABEL: _or8:
; CHECK:       or l
  %c = or i8 %a, %b
  ret i8 %c
}

define i8 @xor8(i8 %a, i8 %b) {
; CHECK-LABEL: _xor8:
; CHECK:       xor l
  %c = xor i8 %a, %b
  ret i8 %c
}

; 16-bit bitwise operations (narrowed to two 8-bit ops by legalizer)
define i16 @and16(i16 %a, i16 %b) {
; CHECK-LABEL: _and16:
; CHECK:       and
; CHECK:       and
  %c = and i16 %a, %b
  ret i16 %c
}

define i16 @or16(i16 %a, i16 %b) {
; CHECK-LABEL: _or16:
; CHECK:       or
; CHECK:       or
  %c = or i16 %a, %b
  ret i16 %c
}

define i16 @xor16(i16 %a, i16 %b) {
; CHECK-LABEL: _xor16:
; CHECK:       xor
; CHECK:       xor
  %c = xor i16 %a, %b
  ret i16 %c
}
