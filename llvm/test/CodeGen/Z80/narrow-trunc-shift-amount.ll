; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 < %s -o /dev/null
; RUN: llc -mtriple=z80 -O1 -stop-after=legalizer < %s -o - | FileCheck %s

; The shift amount is clamped to a byte, so the truncation reaching the
; legalizer takes a byte out of a doubleword. Narrowing the source to halfwords
; leaves the low half still a halfword, which the byte result comes out of.

; CHECK-LABEL: name: shift_by_wide_amount
; CHECK: G_TRUNC
; CHECK-NOT: (s8) = COPY %{{[0-9]+}}(s16)

define i32 @shift_by_wide_amount(i16 %amt, i32 %val) {
entry:
  %wide = zext i16 %amt to i32
  %shr = lshr i32 %val, %wide
  ret i32 %shr
}
