; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 < %s | FileCheck %s --check-prefixes=CHECK,Z80
; RUN: llc -verify-machineinstrs -mtriple=sm83 -O1 < %s | FileCheck %s --check-prefixes=CHECK,SM83

; An unsigned order against a constant takes the constant as immediates
; rather than in pairs, at any width. When the low byte of a 16-bit constant
; is zero the low bytes cannot borrow, and the high byte decides alone.

declare void @hit()

define void @below_page(i16 %x) {
; CHECK-LABEL: below_page:
; Z80:           ld a,h
; SM83:          ld a,d
; CHECK-NEXT:    cp 172
; CHECK-NEXT:    jr nc,
  %c = icmp ult i16 %x, 44032
  br i1 %c, label %t, label %f
t:
  call void @hit()
  ret void
f:
  ret void
}

define void @below(i16 %x) {
; CHECK-LABEL: below:
; Z80:           ld a,l
; SM83:          ld a,e
; CHECK-NEXT:    sub 164
; Z80-NEXT:      ld a,h
; SM83-NEXT:     ld a,d
; CHECK-NEXT:    sbc a,43
; CHECK-NEXT:    jr nc,
  %c = icmp ult i16 %x, 11172
  br i1 %c, label %t, label %f
t:
  call void @hit()
  ret void
f:
  ret void
}

; c < x is x >= c + 1.
define void @above(i16 %x) {
; CHECK-LABEL: above:
; Z80:           ld a,h
; SM83:          ld a,d
; CHECK-NEXT:    cp 19
; CHECK-NEXT:    jr c,
  %c = icmp ugt i16 %x, 4863
  br i1 %c, label %t, label %f
t:
  call void @hit()
  ret void
f:
  ret void
}

define i8 @below_value(i16 %x) {
; CHECK-LABEL: below_value:
; CHECK-NOT:     ld bc,
; CHECK:         sub 95
; CHECK:         sbc a,0
; CHECK-NEXT:    sbc a,a
; CHECK-NEXT:    and 1
  %c = icmp ult i16 %x, 95
  %z = zext i1 %c to i8
  ret i8 %z
}

; A wider value subtracts pair by pair with the borrow, each against its part
; of the constant: 100000 is 0x000186A0.
define void @below_wide(i32 %x) {
; CHECK-LABEL: below_wide:
; CHECK-NOT:     ld bc,
; CHECK-NOT:     ld de,
; CHECK:         sub 160
; CHECK-NEXT:    ld a,{{[a-l]}}
; CHECK-NEXT:    sbc a,134
; CHECK-NEXT:    ld a,{{[a-l]}}
; CHECK-NEXT:    sbc a,1
; CHECK-NEXT:    ld a,{{[a-l]}}
; CHECK-NEXT:    sbc a,0
; CHECK-NEXT:    jr nc,
  %c = icmp ult i32 %x, 100000
  br i1 %c, label %t, label %f
t:
  call void @hit()
  ret void
f:
  ret void
}

; x > 5 is x >= 6 at this width too.
define i8 @above_wide_value(i32 %x) {
; CHECK-LABEL: above_wide_value:
; CHECK:         sub 6
; CHECK-NEXT:    ld a,{{[a-l]}}
; CHECK-NEXT:    sbc a,0
; CHECK-NEXT:    ld a,{{[a-l]}}
; CHECK-NEXT:    sbc a,0
; CHECK-NEXT:    ld a,{{[a-l]}}
; CHECK-NEXT:    sbc a,0
; CHECK-NEXT:    sbc a,a
; CHECK-NEXT:    inc a
  %c = icmp ugt i32 %x, 5
  %z = zext i1 %c to i8
  ret i8 %z
}
