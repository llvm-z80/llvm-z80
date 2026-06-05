; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s

; Test: calling with 3 i8 args (1st→A, 2nd→L, 3rd→stack)
; Verifies LD_r8_n pseudo: ld l,#2 instead of ld a,#2; ld l,a
declare i8 @callee_i8(i8, i8, i8)

define i8 @call_i8_args() {
; CHECK-LABEL: _call_i8_args:
; CHECK:       ld a,#1
; CHECK-NEXT:  ld l,#2
; CHECK-NEXT:  call _callee_i8
  %r = call i8 @callee_i8(i8 1, i8 2, i8 3)
  ret i8 %r
}

; Test: calling with 3 i16 args (1st→HL, 2nd→DE, 3rd→stack)
declare i16 @callee_i16(i16, i16, i16)

define i16 @call_i16_args() {
; CHECK-LABEL: _call_i16_args:
; CHECK:       ld hl,#30
; CHECK-NEXT:  push hl
; CHECK-NEXT:  ld hl,#10
; CHECK-NEXT:  ld de,#20
; CHECK-NEXT:  call _callee_i16
  %r = call i16 @callee_i16(i16 10, i16 20, i16 30)
  ret i16 %r
}

; Test: calling with 4 i16 args (3rd and 4th on stack, right-to-left)
declare i16 @callee_4args(i16, i16, i16, i16)

define i16 @call_i16_4args() {
; CHECK-LABEL: _call_i16_4args:
; CHECK:       ld hl,#400
; CHECK-NEXT:  push hl
; CHECK-NEXT:  ld hl,#300
; CHECK-NEXT:  push hl
; CHECK-NEXT:  ld hl,#100
; CHECK-NEXT:  ld de,#200
; CHECK-NEXT:  call _callee_4args
  %r = call i16 @callee_4args(i16 100, i16 200, i16 300, i16 400)
  ret i16 %r
}

; Test: i8 callee body with signext (tests SPILL_GR8/RELOAD_GR8)
; The spilled arg1 must be reloaded correctly, not dropped
define signext i8 @add3_i8(i8 signext %a, i8 signext %b, i8 signext %c) noinline {
; CHECK-LABEL: _add3_i8:
; CHECK:       add a,b
  %ab = add i8 %b, %a
  %abc = add i8 %ab, %c
  ret i8 %abc
}
