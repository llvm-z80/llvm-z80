; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s
;
; A return value too large for registers comes back through a hidden pointer.
; Under __smallc (cc129) the declared args go left-to-right then the sret
; pointer is pushed last.  Under __sdcccall(0) args go right-to-left then
; the sret pointer is pushed.  Getting the end of that order wrong silently
; hands the callee one of its arguments as the return slot, so the slot each
; push carries is pinned here rather than just the push count.

declare cc129 i64 @mk_smallc(i16, i16)
declare cc128 i64 @mk_sdcc0(i16, i16)

; __smallc: arg1 (4369) pushed first, arg2 (8738) second, sret pointer last.
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

; __sdcccall(0): arg2 (8738) pushed first, arg1 (4369) second, sret pointer
; last, exactly as above.  Only the order of the two declared arguments
; differs between the two conventions.
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

; The matching callee side.  With 2 + 2 + 2 bytes of incoming stack the hidden
; pointer sits at SP+2, %b at SP+4 and %a at SP+6: the declared arguments are
; mirrored above the pointer rather than around it.  The prologue claims six
; bytes of frame first, so each read is six higher than its incoming offset.
; CHECK-LABEL: _def_smallc:
; CHECK:      push af
; CHECK-NEXT: push af
; CHECK-NEXT: push af
; %a, deepest.
; CHECK-NEXT: ld hl,#12
; CHECK-NEXT: add hl,sp
; %b, above it.
; CHECK:      ld hl,#10
; CHECK-NEXT: add hl,sp
; The hidden pointer, nearest the return address.
; CHECK:      ld hl,#8
; CHECK-NEXT: add hl,sp
define cc129 i64 @def_smallc(i16 %a, i16 %b) {
  %x = zext i16 %a to i64
  %y = zext i16 %b to i64
  %r = add i64 %x, %y
  ret i64 %r
}
