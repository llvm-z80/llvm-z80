; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s

; Test: void function returns with just ret
define void @void_return() {
; CHECK-LABEL: _void_return:
; CHECK:       ret
  ret void
}

; Test: 8-bit constant return in A
define i8 @return_const8() {
; CHECK-LABEL: _return_const8:
; CHECK:       ld a,#42
; CHECK-NEXT:  ret
  ret i8 42
}

; Test: 16-bit constant return in DE (SDCC __sdcccall(1))
define i16 @return_const16() {
; CHECK-LABEL: _return_const16:
; CHECK:       ld de,#1234
; CHECK-NEXT:  ret
  ret i16 1234
}

; Test: return second 16-bit arg (DE) - already in return register
define i16 @return_second(i16 %a, i16 %b) {
; CHECK-LABEL: _return_second:
; CHECK:       ret
  ret i16 %b
}

; Test: return third 16-bit arg (stack in SDCC, only 2 reg params)
define i16 @return_third(i16 %a, i16 %b, i16 %c) {
; CHECK-LABEL: _return_third:
; CHECK:       ld hl,#2
; CHECK-NEXT:  add hl,sp
; CHECK-NEXT:  ld e,(hl)
; CHECK-NEXT:  inc hl
; CHECK-NEXT:  ld d,(hl)
  ret i16 %c
}

; Test: 32-bit return identity (HLDE passthrough)
define i32 @return32(i32 %a) {
; CHECK-LABEL: _return32:
; CHECK:       ret
  ret i32 %a
}

; Test: 4th argument comes from stack (3rd and 4th on stack)
define i16 @stack_arg(i16 %a, i16 %b, i16 %c, i16 %d) {
; CHECK-LABEL: _stack_arg:
; CHECK:       ld hl,#4
; CHECK-NEXT:  add hl,sp
; CHECK-NEXT:  ld e,(hl)
; CHECK-NEXT:  inc hl
; CHECK-NEXT:  ld d,(hl)
  ret i16 %d
}

; Test: function call
declare void @external_func(i16)

define void @call_func(i16 %x) {
; CHECK-LABEL: _call_func:
; CHECK:       call _external_func
  call void @external_func(i16 %x)
  ret void
}
