; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s

define i8 @shl8(i8 %a) {
; CHECK-LABEL: _shl8:
; CHECK:       add a,a
; CHECK-NEXT:  ret
  %c = shl i8 %a, 1
  ret i8 %c
}

define i16 @shl16(i16 %a) {
; CHECK-LABEL: _shl16:
; CHECK:       add hl,hl
; CHECK-NEXT:  ex de,hl
; CHECK-NEXT:  ret
  %c = shl i16 %a, 1
  ret i16 %c
}

define i8 @lshr8(i8 %a) {
; CHECK-LABEL: _lshr8:
; CHECK:       srl a
; CHECK-NEXT:  ret
  %c = lshr i8 %a, 1
  ret i8 %c
}

define i16 @lshr16(i16 %a) {
; CHECK-LABEL: _lshr16:
; CHECK:       srl d
; CHECK-NEXT:  rr e
; CHECK-NEXT:  ret
  %c = lshr i16 %a, 1
  ret i16 %c
}

define i8 @ashr8(i8 %a) {
; CHECK-LABEL: _ashr8:
; CHECK:       sra a
; CHECK-NEXT:  ret
  %c = ashr i8 %a, 1
  ret i8 %c
}

define i16 @ashr16(i16 %a) {
; CHECK-LABEL: _ashr16:
; CHECK:       sra d
; CHECK-NEXT:  rr e
; CHECK-NEXT:  ret
  %c = ashr i16 %a, 1
  ret i16 %c
}
