; RUN: llc -verify-machineinstrs -mtriple=sm83 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=SM83
; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=Z80

; A byte shifted right takes the shortest sequence: SRL for each bit, two
; bytes apiece, or past the middle of the byte a rotate the other way and a
; mask, one byte per rotate. SM83's SWAP moves a nibble at once.

define i8 @lshr3(i8 %a) {
; SM83-LABEL: _lshr3:
; SM83:       srl a
; SM83-NEXT:  srl a
; SM83-NEXT:  srl a
; Z80-LABEL: _lshr3:
; Z80:        srl a
; Z80-NEXT:   srl a
; Z80-NEXT:   srl a
  %r = lshr i8 %a, 3
  ret i8 %r
}

define i8 @lshr4(i8 %a) {
; SM83-LABEL: _lshr4:
; SM83:       swap a
; SM83-NEXT:  and #15
; Z80-LABEL: _lshr4:
; Z80:        rlca
; Z80-NEXT:   rlca
; Z80-NEXT:   rlca
; Z80-NEXT:   rlca
; Z80-NEXT:   and #15
  %r = lshr i8 %a, 4
  ret i8 %r
}

define i8 @lshr5(i8 %a) {
; SM83-LABEL: _lshr5:
; SM83:       rlca
; SM83-NEXT:  rlca
; SM83-NEXT:  rlca
; SM83-NEXT:  and #7
; Z80-LABEL: _lshr5:
; Z80:        rlca
; Z80-NEXT:   rlca
; Z80-NEXT:   rlca
; Z80-NEXT:   and #7
  %r = lshr i8 %a, 5
  ret i8 %r
}

define i8 @lshr7(i8 %a) {
; SM83-LABEL: _lshr7:
; SM83:       rlca
; SM83-NEXT:  and #1
; Z80-LABEL: _lshr7:
; Z80:        rlca
; Z80-NEXT:   and #1
  %r = lshr i8 %a, 7
  ret i8 %r
}

; To the left, ADD A,A doubles in a single byte, and the rotate and mask
; only pay from six bits on.

define i8 @shl5(i8 %a) {
; SM83-LABEL: _shl5:
; SM83:       add a,a
; SM83-NEXT:  add a,a
; SM83-NEXT:  add a,a
; SM83-NEXT:  add a,a
; SM83-NEXT:  add a,a
; Z80-LABEL: _shl5:
; Z80:        add a,a
; Z80-NEXT:   add a,a
; Z80-NEXT:   add a,a
; Z80-NEXT:   add a,a
; Z80-NEXT:   add a,a
  %r = shl i8 %a, 5
  ret i8 %r
}

define i8 @shl6(i8 %a) {
; SM83-LABEL: _shl6:
; SM83:       rrca
; SM83-NEXT:  rrca
; SM83-NEXT:  and #192
; Z80-LABEL: _shl6:
; Z80:        rrca
; Z80-NEXT:   rrca
; Z80-NEXT:   and #192
  %r = shl i8 %a, 6
  ret i8 %r
}

; A pair shifted by a byte or more is its other byte shifted on its own, with
; the vacated byte zero.

define i16 @lshr16_13(i16 %a) {
; SM83-LABEL: _lshr16_13:
; SM83:       rlca
; SM83-NEXT:  rlca
; SM83-NEXT:  rlca
; SM83-NEXT:  and #7
; Z80-LABEL: _lshr16_13:
; Z80:        rlca
; Z80-NEXT:   rlca
; Z80-NEXT:   rlca
; Z80-NEXT:   and #7
  %r = lshr i16 %a, 13
  ret i16 %r
}
