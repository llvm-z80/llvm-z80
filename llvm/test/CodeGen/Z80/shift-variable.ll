; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s

; Test: variable 8-bit shift left uses DJNZ loop
define i8 @shl8_var(i8 %val, i8 %amt) {
; CHECK-LABEL: _shl8_var:
; CHECK:       inc b
; CHECK-NEXT:  dec b
; CHECK-NEXT:  jr z,
; CHECK:       sla a
; CHECK-NEXT:  djnz
  %r = shl i8 %val, %amt
  ret i8 %r
}

; Test: variable 8-bit logical shift right
define i8 @lshr8_var(i8 %val, i8 %amt) {
; CHECK-LABEL: _lshr8_var:
; CHECK:       inc b
; CHECK-NEXT:  dec b
; CHECK-NEXT:  jr z,
; CHECK:       srl a
; CHECK-NEXT:  djnz
  %r = lshr i8 %val, %amt
  ret i8 %r
}

; Test: variable 8-bit arithmetic shift right
define i8 @ashr8_var(i8 %val, i8 %amt) {
; CHECK-LABEL: _ashr8_var:
; CHECK:       inc b
; CHECK-NEXT:  dec b
; CHECK-NEXT:  jr z,
; CHECK:       sra a
; CHECK-NEXT:  djnz
  %r = ashr i8 %val, %amt
  ret i8 %r
}

; Test: variable 16-bit shift left uses ADD HL,HL in loop
define i16 @shl16_var(i16 %val, i8 %amt) {
; CHECK-LABEL: _shl16_var:
; CHECK:       inc b
; CHECK-NEXT:  dec b
; CHECK-NEXT:  jr z,
; CHECK:       add hl,hl
; CHECK-NEXT:  djnz
  %ext = zext i8 %amt to i16
  %r = shl i16 %val, %ext
  ret i16 %r
}

; Test: variable 16-bit logical shift right uses SRL H; RR L
define i16 @lshr16_var(i16 %val, i8 %amt) {
; CHECK-LABEL: _lshr16_var:
; CHECK:       inc b
; CHECK-NEXT:  dec b
; CHECK-NEXT:  jr z,
; CHECK:       srl h
; CHECK-NEXT:  rr l
; CHECK-NEXT:  djnz
  %ext = zext i8 %amt to i16
  %r = lshr i16 %val, %ext
  ret i16 %r
}

; Test: variable 16-bit arithmetic shift right uses SRA H; RR L
define i16 @ashr16_var(i16 %val, i8 %amt) {
; CHECK-LABEL: _ashr16_var:
; CHECK:       inc b
; CHECK-NEXT:  dec b
; CHECK-NEXT:  jr z,
; CHECK:       sra h
; CHECK-NEXT:  rr l
; CHECK-NEXT:  djnz
  %ext = zext i8 %amt to i16
  %r = ashr i16 %val, %ext
  ret i16 %r
}
