; RUN: llc -verify-machineinstrs -mtriple=sm83 -O2 < %s | FileCheck %s
;
; The outlined body writes C and B before it reads BC, so BC is not an input
; of the call. A call that read BC would read a register with no value here.
; The unused %q puts %n on the stack.

define i8 @f(ptr %p, ptr %q, i16 %n) minsize {
entry:
  br label %loop

loop:
  %a = load i8, ptr %p, align 1
  %za = zext i8 %a to i16
  %ma = mul i16 %n, %za
  %b = load volatile i8, ptr %p, align 1
  %zb = zext i8 %b to i16
  %mb = mul i16 %n, %zb
  %or = or i16 %mb, %ma
  store i16 %or, ptr %p, align 1
  br label %loop
}

; CHECK-LABEL: _f:
; CHECK:       call _OUTLINED_FUNCTION_0
; CHECK:       call _OUTLINED_FUNCTION_0
; CHECK-LABEL: _OUTLINED_FUNCTION_0:
; CHECK:       ld c,a
; CHECK-NEXT:  ld b,(hl)
; CHECK-NEXT:  ld a,(bc)
