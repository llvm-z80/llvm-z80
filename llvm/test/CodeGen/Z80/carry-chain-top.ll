; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 < %s | FileCheck %s --check-prefixes=CHECK,Z80
; RUN: llc -verify-machineinstrs -mtriple=sm83 -O1 < %s | FileCheck %s --check-prefixes=CHECK,SM83

; A 32-bit add or subtract runs as a carry chain over the two halves. The
; carry between them goes through A, but nothing reads the carry out of the
; top half, so it is not captured.

define i32 @add32(i32 %a, i32 %b) {
; CHECK-LABEL: add32:
; CHECK:         sbc a,a
; CHECK-NEXT:    and 1
; CHECK:         rrca
; Z80-NEXT:      adc hl,bc
; SM83-NEXT:     ld a,l
; SM83-NEXT:     adc a,e
; CHECK-NOT:     sbc a,a
; CHECK:         .Lfunc_end0:
  %r = add i32 %a, %b
  ret i32 %r
}

define i32 @sub32(i32 %a, i32 %b) {
; CHECK-LABEL: sub32:
; CHECK:         sbc a,a
; CHECK-NEXT:    and 1
; CHECK:         rrca
; Z80-NEXT:      sbc hl,bc
; SM83-NEXT:     ld a,l
; SM83-NEXT:     sbc a,e
; CHECK-NOT:     sbc a,a
; CHECK:         .Lfunc_end1:
  %r = sub i32 %a, %b
  ret i32 %r
}
