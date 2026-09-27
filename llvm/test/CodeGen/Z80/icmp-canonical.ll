; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 < %s | FileCheck %s --check-prefixes=CHECK,Z80
; RUN: llc -verify-machineinstrs -mtriple=sm83 -O1 < %s | FileCheck %s --check-prefixes=CHECK,SM83

; The legalizer puts a compare in the forms the selector takes directly: on a
; byte or a pair, x > c as x >= c + 1 and x <= c as x < c + 1, so that c
; stays an immediate on the right; a signed order as the unsigned one with the sign bit
; of both sides flipped; and x < 0 or x >= 0, at any width, as a test of the
; sign bit alone.

declare void @g()

; The sign bit shifted out of A, when the value dies with the test.
define void @sign_branch(i8 %x) {
; CHECK-LABEL: sign_branch:
; CHECK:         add a,a
; CHECK-NEXT:    jr c,
  %c = icmp slt i8 %x, 0
  br i1 %c, label %t, label %f
t:
  call void @g()
  br label %f
f:
  ret void
}

; Read in place when the value is still needed.
define i8 @sign_branch_live(i8 %x) {
; CHECK-LABEL: sign_branch_live:
; CHECK:         bit 7,a
; CHECK-NEXT:    jr nz,
  %c = icmp sgt i8 %x, -1
  br i1 %c, label %t, label %f
t:
  call void @g()
  br label %f
f:
  ret i8 %x
}

define i8 @sign_value(i8 %x) {
; CHECK-LABEL: sign_value:
; CHECK:         rlca
; CHECK-NEXT:    and 1
; CHECK-NEXT:    ret
  %c = icmp slt i8 %x, 0
  %r = zext i1 %c to i8
  ret i8 %r
}

define i8 @nonneg_value(i8 %x) {
; CHECK-LABEL: nonneg_value:
; CHECK:         cpl
; CHECK-NEXT:    rlca
; CHECK-NEXT:    and 1
; CHECK-NEXT:    ret
  %c = icmp sgt i8 %x, -1
  %r = zext i1 %c to i8
  ret i8 %r
}

; The constant is flipped here: 5 ^ 0x80.
define i8 @signed_const(i8 %x) {
; CHECK-LABEL: signed_const:
; CHECK:         xor 128
; CHECK-NEXT:    cp 133
; CHECK-NEXT:    sbc a,a
; CHECK-NEXT:    and 1
; CHECK-NEXT:    ret
  %c = icmp slt i8 %x, 5
  %r = zext i1 %c to i8
  ret i8 %r
}

; x > 5 is x >= 6, flipped to 134; the result of >= is -1 + 1 on a borrow.
define i8 @signed_const_gt(i8 %x) {
; CHECK-LABEL: signed_const_gt:
; CHECK:         xor 128
; CHECK-NEXT:    cp 134
; CHECK-NEXT:    sbc a,a
; CHECK-NEXT:    inc a
; CHECK-NEXT:    ret
  %c = icmp sgt i8 %x, 5
  %r = zext i1 %c to i8
  ret i8 %r
}

define void @unsigned_gt_branch(i8 %x) {
; CHECK-LABEL: unsigned_gt_branch:
; CHECK:         cp 6
; CHECK-NEXT:    jr c,
  %c = icmp ugt i8 %x, 5
  br i1 %c, label %t, label %f
t:
  call void @g()
  br label %f
f:
  ret void
}

define i8 @unsigned_le_value(i8 %x) {
; CHECK-LABEL: unsigned_le_value:
; CHECK:         cp 6
; CHECK-NEXT:    sbc a,a
; CHECK-NEXT:    and 1
; CHECK-NEXT:    ret
  %c = icmp ule i8 %x, 5
  %r = zext i1 %c to i8
  ret i8 %r
}

; The sign of a pair is in its high byte.
define void @pair_sign_branch(i16 %x) {
; CHECK-LABEL: pair_sign_branch:
; Z80:           ld a,h
; SM83:          ld a,d
; CHECK-NEXT:    add a,a
; CHECK-NEXT:    jr c,
  %c = icmp slt i16 %x, 0
  br i1 %c, label %t, label %f
t:
  call void @g()
  br label %f
f:
  ret void
}

; x > 5 is x >= 6, and 6 ^ 0x8000 is taken as immediates.
define void @pair_signed_const(i16 %x) {
; CHECK-LABEL: pair_signed_const:
; Z80:           ld a,h
; SM83:          ld a,d
; CHECK-NEXT:    xor 128
; CHECK:         sub 6
; CHECK-NEXT:    ld a,{{[hd]}}
; CHECK-NEXT:    sbc a,128
; CHECK-NEXT:    jr c,
  %c = icmp sgt i16 %x, 5
  br i1 %c, label %t, label %f
t:
  call void @g()
  br label %f
f:
  ret void
}

define i8 @pair_signed(i16 %x, i16 %y) {
; CHECK-LABEL: pair_signed:
; CHECK:         xor 128
; CHECK:         xor 128
; Z80:           ld a,l
; Z80-NEXT:      sub e
; Z80-NEXT:      ld a,h
; Z80-NEXT:      sbc a,d
; SM83:          ld a,e
; SM83-NEXT:     sub c
; SM83-NEXT:     ld a,d
; SM83-NEXT:     sbc a,b
; CHECK-NEXT:    sbc a,a
; CHECK-NEXT:    and 1
  %c = icmp slt i16 %x, %y
  %r = zext i1 %c to i8
  ret i8 %r
}

; A signed order on pointers is one on their addresses.
define i8 @address_sign(ptr %p) {
; CHECK-LABEL: address_sign:
; Z80:           ld a,h
; SM83:          ld a,d
; CHECK-NEXT:    rlca
; CHECK-NEXT:    and 1
; CHECK-NEXT:    ret
  %c = icmp slt ptr %p, null
  %r = zext i1 %c to i8
  ret i8 %r
}

; A wider value has its sign in the top byte too, and x > -1 is x >= 0.
define void @wide_sign_branch(i32 %x) {
; CHECK-LABEL: wide_sign_branch:
; Z80:           ld a,h
; SM83:          ld a,d
; CHECK-NEXT:    add a,a
; CHECK-NEXT:    jr c,
  %c = icmp slt i32 %x, 0
  br i1 %c, label %t, label %f
t:
  call void @g()
  br label %f
f:
  ret void
}

define i8 @wide_nonneg_value(i32 %x) {
; CHECK-LABEL: wide_nonneg_value:
; Z80:           ld a,h
; SM83:          ld a,d
; CHECK-NEXT:    cpl
; CHECK-NEXT:    rlca
; CHECK-NEXT:    and 1
; CHECK-NEXT:    ret
  %c = icmp sgt i32 %x, -1
  %r = zext i1 %c to i8
  ret i8 %r
}
