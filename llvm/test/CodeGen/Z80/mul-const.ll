; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s | FileCheck %s

; A 16-bit multiply by a constant becomes shifts and adds when that is short,
; and a __mulhi3 call otherwise.

; CHECK-LABEL: _mul10:
; CHECK-NOT:   call
; CHECK:       add hl,hl
; CHECK-NOT:   call
; CHECK:       ret
define i16 @mul10(i16 %x) {
  %r = mul i16 %x, 10
  ret i16 %r
}

; Eight zero bits shift in at once, which moves a byte.
; CHECK-LABEL: _mul257:
; CHECK-NOT:   call
; CHECK-NOT:   add hl,hl
; CHECK:       ret
define i16 @mul257(i16 %x) {
  %r = mul i16 %x, 257
  ret i16 %r
}

; CHECK-LABEL: _mul1000:
; CHECK:       jp ___mulhi3
define i16 @mul1000(i16 %x) {
  %r = mul i16 %x, 1000
  ret i16 %r
}
