; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s
;
; A return value too large for registers comes back through a hidden pointer.
; That pointer stays out of the __smallc order reversal: SDCC 4.6.0 pushes it
; LAST under __smallc just as it does under __sdcccall(0), leaving it in the
; slot right above the return address with the mirrored declared arguments
; stacked on top.  Getting the end wrong silently hands the callee one of its
; arguments as the return slot.

declare cc129 i64 @mk_smallc(i16, i16)
declare cc128 i64 @mk_sdcc0(i16, i16)

; __smallc: the two declared arguments go first-to-last, and the hidden
; pointer follows them.
; CHECK-LABEL: _call_smallc:
; CHECK:      ld hl,#4369
; CHECK-NEXT: push hl
; CHECK-NEXT: ld hl,#8738
; CHECK-NEXT: push hl
; CHECK-NEXT: ld hl,#12
; CHECK-NEXT: add hl,sp
; CHECK-NEXT: push hl
; CHECK-NEXT: call _mk_smallc
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

; The matching callee side: with 2 + 2 + 2 bytes of incoming stack, the hidden
; pointer is at SP+2, %b at SP+4 and %a at SP+6, the two declared arguments
; mirrored above the pointer rather than around it.  The prologue claims six
; bytes of frame first, so each of those reads six higher than its incoming
; offset: %a at 12, %b at 10, the hidden pointer at 8.
; CHECK-LABEL: _def_smallc:
; CHECK:      push af
; CHECK-NEXT: push af
; CHECK-NEXT: push af
; CHECK:      ld hl,#12
; CHECK-NEXT: add hl,sp
; CHECK:      ld hl,#10
; CHECK-NEXT: add hl,sp
; CHECK:      ld hl,#8
; CHECK-NEXT: add hl,sp
define cc129 i64 @def_smallc(i16 %a, i16 %b) {
  %x = zext i16 %a to i64
  %y = zext i16 %b to i64
  %r = add i64 %x, %y
  ret i64 %r
}
