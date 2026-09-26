; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 < %s | FileCheck %s --check-prefixes=CHECK,Z80
; RUN: llc -verify-machineinstrs -mtriple=sm83 -O1 < %s | FileCheck %s --check-prefixes=CHECK,SM83

; An equality compare leaves A zero exactly when its sides are equal, for a
; branch on Z and a 0/1 value alike: SUB 1 borrows only from zero and
; ADD 0xFF carries from anything else.

declare void @g()

; A byte compared with zero is its own difference.
define i8 @byte_zero(i8 %x) {
; CHECK-LABEL: byte_zero:
; CHECK-NOT:     sub 0
; CHECK:         sub 1
; CHECK-NEXT:    sbc a,a
; CHECK-NEXT:    and 1
  %c = icmp eq i8 %x, 0
  %z = zext i1 %c to i8
  ret i8 %z
}

define i8 @byte_const(i8 %x) {
; CHECK-LABEL: byte_const:
; CHECK:         sub 7
; CHECK-NEXT:    add a,255
; CHECK-NEXT:    sbc a,a
; CHECK-NEXT:    and 1
  %c = icmp ne i8 %x, 7
  %z = zext i1 %c to i8
  ret i8 %z
}

; A branch needs only the flag: CP keeps the byte in A.
define void @byte_branch(i8 %x) {
; CHECK-LABEL: byte_branch:
; CHECK:         cp 7
; CHECK-NEXT:    jr nz,
  %c = icmp eq i8 %x, 7
  br i1 %c, label %t, label %f
t:
  call void @g()
  br label %f
f:
  ret void
}

; A pair is equal when the differences of its bytes OR to zero, and a byte
; compared with zero goes into the OR as it is.
define i8 @pair_zero(i16 %x) {
; CHECK-LABEL: pair_zero:
; Z80:           ld a,l
; Z80-NEXT:      or h
; SM83:          ld a,e
; SM83-NEXT:     or d
; CHECK-NEXT:    sub 1
; CHECK-NEXT:    sbc a,a
; CHECK-NEXT:    and 1
  %c = icmp eq i16 %x, 0
  %z = zext i1 %c to i8
  ret i8 %z
}

define i8 @pair_low_const(i16 %x) {
; CHECK-LABEL: pair_low_const:
; Z80:           ld a,l
; SM83:          ld a,e
; CHECK-NEXT:    xor 5
; Z80-NEXT:      or h
; SM83-NEXT:     or d
; CHECK-NEXT:    add a,255
  %c = icmp ne i16 %x, 5
  %z = zext i1 %c to i8
  ret i8 %z
}

; The same in a branch, with the zero byte low.
define void @pair_high_const(i16 %x) {
; CHECK-LABEL: pair_high_const:
; Z80:           ld a,h
; SM83:          ld a,d
; CHECK-NEXT:    xor 5
; Z80-NEXT:      or l
; SM83-NEXT:     or e
; CHECK-NEXT:    jr nz,
  %c = icmp eq i16 %x, 1280
  br i1 %c, label %t, label %f
t:
  call void @g()
  br label %f
f:
  ret void
}

; With no zero byte, the high difference waits in a register of its own.
define i8 @pair_const(i16 %x) {
; CHECK-LABEL: pair_const:
; CHECK:         xor 18
; CHECK-NEXT:    ld [[T:[a-l]]],a
; CHECK-NEXT:    ld a,{{[le]}}
; CHECK-NEXT:    xor 52
; CHECK-NEXT:    or [[T]]
; CHECK-NEXT:    sub 1
  %c = icmp eq i16 %x, 4660
  %z = zext i1 %c to i8
  ret i8 %z
}

define i8 @pair_var(i16 %x, i16 %y) {
; CHECK-LABEL: pair_var:
; CHECK:         xor {{[a-l]}}
; CHECK-NEXT:    ld [[T:[a-l]]],a
; CHECK-NEXT:    ld a,{{[le]}}
; CHECK-NEXT:    xor {{[a-l]}}
; CHECK-NEXT:    or [[T]]
; CHECK-NEXT:    sub 1
  %c = icmp eq i16 %x, %y
  %z = zext i1 %c to i8
  ret i8 %z
}

; A wider value ORs the tests of its pairs together.
define i8 @wide_zero(i32 %x) {
; CHECK-LABEL: wide_zero:
; CHECK:         or {{[a-l]}}
; CHECK-NEXT:    ld [[T:[a-l]]],a
; CHECK-NEXT:    ld a,{{[a-l]}}
; CHECK-NEXT:    or {{[a-l]}}
; CHECK-NEXT:    or [[T]]
; CHECK-NEXT:    sub 1
  %c = icmp eq i32 %x, 0
  %z = zext i1 %c to i8
  ret i8 %z
}
