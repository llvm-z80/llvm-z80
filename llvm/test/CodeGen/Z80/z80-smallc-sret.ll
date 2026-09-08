; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s
;
; A return value too large for registers comes back through a hidden pointer.
; Under __smallc (cc129) the declared args go left-to-right then the sret
; pointer is pushed last.  Under __sdcccall(0) args go right-to-left then
; the sret pointer is pushed.

declare cc129 i64 @mk_smallc(i16, i16)
declare cc128 i64 @mk_sdcc0(i16, i16)

; __smallc: arg1 (4369) pushed first, arg2 (8738) pushed second, sret pointer pushed.
; CHECK-LABEL: _call_smallc:
; CHECK:      ld hl,#4369
; CHECK:      push hl
; CHECK:      ld hl,#8738
; CHECK:      push hl
; CHECK:      add hl,sp
; CHECK:      push hl
; CHECK:      call _mk_smallc
define void @call_smallc(ptr %out) {
  %r = call cc129 i64 @mk_smallc(i16 4369, i16 8738)
  store i64 %r, ptr %out
  ret void
}

; __sdcccall(0): arg2 (8738) pushed first, arg1 (4369) pushed second, sret pointer pushed.
; CHECK-LABEL: _call_sdcc0:
; CHECK:      ld hl,#8738
; CHECK:      push hl
; CHECK:      ld hl,#4369
; CHECK:      push hl
; CHECK:      add hl,sp
; CHECK:      push hl
; CHECK:      call _mk_sdcc0
define void @call_sdcc0(ptr %out) {
  %r = call cc128 i64 @mk_sdcc0(i16 4369, i16 8738)
  store i64 %r, ptr %out
  ret void
}

; The callee receives %a and %b via the stack; only the sret pointer is on the
; stack alongside them.
; CHECK-LABEL: _def_smallc:
; CHECK:      push af
; CHECK:      push af
; CHECK:      push af
define cc129 i64 @def_smallc(i16 %a, i16 %b) {
  %x = zext i16 %a to i64
  %y = zext i16 %b to i64
  %r = add i64 %x, %y
  ret i64 %r
}
