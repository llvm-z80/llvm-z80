; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 < %s | FileCheck %s

; An adjacent i32 x/y and x%y on identical operands must fuse into ONE
; runtime call (__udivmodsi4, quotient returned + remainder via a caller
; pointer), not two separate __udivsi3 + __umodsi3 calls that each re-run
; the full 32-bit division core.
;
; C source reproducing the pattern:
;   unsigned add_qr(unsigned a, unsigned b) { return a / b + a % b; }

define i32 @udivmod32(i32 %a, i32 %b) {
; CHECK-LABEL: _udivmod32:
; CHECK:       call ___udivmodsi4
; CHECK-NOT:   ___udivsi3
; CHECK-NOT:   ___umodsi3
  %q = udiv i32 %a, %b
  %r = urem i32 %a, %b
  %s = add i32 %q, %r
  ret i32 %s
}

; Only the low 16 bits of the remainder are used: still one fused call; the
; dead high half of the remainder may be optimized away.
define i16 @udivmod32_narrow_rem(i32 %a, i32 %b, ptr %out) {
; CHECK-LABEL: _udivmod32_narrow_rem:
; CHECK:       call ___udivmodsi4
; CHECK-NOT:   ___udivsi3
; CHECK-NOT:   ___umodsi3
  %q = udiv i32 %a, %b
  %r = urem i32 %a, %b
  store i32 %q, ptr %out
  %rt = trunc i32 %r to i16
  ret i16 %rt
}

; Signed i32 divrem fuses too, into __divmodsi4 (one call), not separate
; __divsi3 + __modsi3.
define i32 @sdivmod32(i32 %a, i32 %b) {
; CHECK-LABEL: _sdivmod32:
; CHECK:       call ___divmodsi4
; CHECK-NOT:   ___divsi3
; CHECK-NOT:   ___modsi3
  %q = sdiv i32 %a, %b
  %r = srem i32 %a, %b
  %s = add i32 %q, %r
  ret i32 %s
}
