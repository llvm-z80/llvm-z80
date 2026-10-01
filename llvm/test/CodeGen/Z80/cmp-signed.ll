; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s

; A signed order is the unsigned one with the sign bit of both sides flipped,
; which for pairs is in their high bytes.

; Test: signed less-than (SLT)
define i8 @icmp_slt16(i16 %a, i16 %b) {
; CHECK-LABEL: icmp_slt16:
; CHECK:       ld a,d
; CHECK-NEXT:  xor #128
; CHECK:       ld a,h
; CHECK-NEXT:  xor #128
; CHECK:       sub e
; CHECK-NEXT:  ld a,h
; CHECK-NEXT:  sbc a,d
; CHECK-NEXT:  sbc a,a
; CHECK-NEXT:  and #1
  %c = icmp slt i16 %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; Test: signed greater-or-equal (SGE) - the inverse of SLT
define i8 @icmp_sge16(i16 %a, i16 %b) {
; CHECK-LABEL: icmp_sge16:
; CHECK:       xor #128
; CHECK:       xor #128
; CHECK:       sub e
; CHECK-NEXT:  ld a,h
; CHECK-NEXT:  sbc a,d
; CHECK-NEXT:  sbc a,a
; CHECK-NEXT:  inc a
  %c = icmp sge i16 %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; Test: signed less-or-equal (SLE) - swapped operands
define i8 @icmp_sle16(i16 %a, i16 %b) {
; CHECK-LABEL: icmp_sle16:
; CHECK:       xor #128
; CHECK:       xor #128
; CHECK:       sub l
; CHECK-NEXT:  ld a,d
; CHECK-NEXT:  sbc a,h
; CHECK-NEXT:  sbc a,a
; CHECK-NEXT:  inc a
  %c = icmp sle i16 %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; Test: signed greater-than (SGT) - swapped operands
define i8 @icmp_sgt16(i16 %a, i16 %b) {
; CHECK-LABEL: icmp_sgt16:
; CHECK:       xor #128
; CHECK:       xor #128
; CHECK:       sub l
; CHECK-NEXT:  ld a,d
; CHECK-NEXT:  sbc a,h
; CHECK-NEXT:  sbc a,a
; CHECK-NEXT:  and #1
  %c = icmp sgt i16 %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; Test: unsigned less-or-equal (ULE) - swapped operands, 8-bit SUB/SBC chain
define i8 @icmp_ule16(i16 %a, i16 %b) {
; CHECK-LABEL: icmp_ule16:
; CHECK:       sub l
; CHECK:       sbc a,h
; CHECK:       sbc a,a
; CHECK:       inc a
  %c = icmp ule i16 %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}

; Test: unsigned greater-than (UGT) - swapped operands, 8-bit SUB/SBC chain
define i8 @icmp_ugt16(i16 %a, i16 %b) {
; CHECK-LABEL: icmp_ugt16:
; CHECK:       sub l
; CHECK:       sbc a,h
; CHECK:       sbc a,a
; CHECK:       and #1
  %c = icmp ugt i16 %a, %b
  %r = zext i1 %c to i8
  ret i8 %r
}
