; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s
;
; A return value too large for registers comes back through a hidden pointer.
; Under __smallc (cc129) the declared args go in HL/DE; the hidden sret pointer
; is pushed on the stack before loading HL/DE.  Under __sdcccall(0) all args
; and the sret pointer are pushed right-to-left, with the sret pointer last.

declare cc129 i64 @mk_smallc(i16, i16)
declare cc128 i64 @mk_sdcc0(i16, i16)

; __smallc: arg1 in HL, arg2 in DE; sret pointer on the stack.
; CHECK-LABEL: _call_smallc:
; CHECK-DAG:  ld hl,#4369
; CHECK-DAG:  ld de,#8738
; CHECK:      call _mk_smallc
define void @call_smallc(ptr %out) {
  %r = call cc129 i64 @mk_smallc(i16 4369, i16 8738)
  store i64 %r, ptr %out
  ret void
}

; __sdcccall(0): arguments right-to-left, and the hidden pointer pushed last.
; CHECK-LABEL: _call_sdcc0:
; CHECK:      ld hl,#8738
; CHECK-NEXT: push hl
; CHECK-NEXT: ld hl,#4369
; CHECK-NEXT: push hl
; CHECK-NEXT: ld hl,#12
; CHECK-NEXT: add hl,sp
; CHECK-NEXT: push hl
; CHECK-NEXT: call _mk_sdcc0
define void @call_sdcc0(ptr %out) {
  %r = call cc128 i64 @mk_sdcc0(i16 4369, i16 8738)
  store i64 %r, ptr %out
  ret void
}

; The callee receives %a in HL and %b in DE; only the sret pointer is on the
; stack.  The prologue claims a frame; the sret pointer is at SP+8 after that.
; CHECK-LABEL: _def_smallc:
; CHECK:      push af
; CHECK-NEXT: push af
; CHECK-NEXT: push af
; CHECK:      ld hl,#8
; CHECK-NEXT: add hl,sp
define cc129 i64 @def_smallc(i16 %a, i16 %b) {
  %x = zext i16 %a to i64
  %y = zext i16 %b to i64
  %r = add i64 %x, %y
  ret i64 %r
}
