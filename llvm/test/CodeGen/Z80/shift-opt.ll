; RUN: llc -mtriple=sm83 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=SM83
; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=Z80

; Test shift optimizations: SM83 SWAP instruction, RLCA for shift-by-7

define i8 @lshr4(i8 %a) {
; SM83-LABEL: _lshr4:
; SM83:       swap a
; SM83-NEXT:  and #15
; Z80-LABEL: _lshr4:
; Z80:       srl a
; Z80:       srl a
; Z80:       srl a
; Z80:       srl a
  %r = lshr i8 %a, 4
  ret i8 %r
}

define i8 @lshr5(i8 %a) {
; SM83-LABEL: _lshr5:
; SM83:       swap a
; SM83-NEXT:  and #15
; SM83-NEXT:  srl a
; Z80-LABEL: _lshr5:
; Z80:       srl a
; Z80:       srl a
; Z80:       srl a
; Z80:       srl a
; Z80:       srl a
  %r = lshr i8 %a, 5
  ret i8 %r
}

define i8 @lshr6(i8 %a) {
; SM83-LABEL: _lshr6:
; SM83:       swap a
; SM83-NEXT:  and #15
; SM83-NEXT:  srl a
; SM83-NEXT:  srl a
; Z80-LABEL: _lshr6:
; Z80:       srl a
; Z80:       srl a
; Z80:       srl a
; Z80:       srl a
; Z80:       srl a
; Z80:       srl a
  %r = lshr i8 %a, 6
  ret i8 %r
}

define i8 @lshr7(i8 %a) {
; SM83-LABEL: _lshr7:
; SM83:       rlca
; SM83-NEXT:  and #1
; Z80-LABEL: _lshr7:
; Z80:       rlca
; Z80-NEXT:  and #1
  %r = lshr i8 %a, 7
  ret i8 %r
}
