; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 < %s -o /dev/null
; RUN: llc -mtriple=z80 -O1 -stop-after=instruction-select < %s -o - | FileCheck %s

; Comparing a halfword against a small constant reads its two halves one at a
; time. They are named where the value already sits: a move into a fixed pair
; would tie the allocator to that pair and leave a definition of it which the
; halves outlive.

; CHECK-LABEL: name: eq_small_const
; CHECK:      COPY %{{[0-9]+}}.sub_hi
; CHECK:      $a = COPY %{{[0-9]+}}.sub_lo
; CHECK-NEXT: SUB_n 42
; CHECK-NEXT: OR_r
; CHECK-NOT:  $hl = COPY

define i16 @eq_small_const(i16 %v, i16 %a, i16 %b) {
  %c = icmp eq i16 %v, 42
  %r = select i1 %c, i16 %a, i16 %b
  ret i16 %r
}
