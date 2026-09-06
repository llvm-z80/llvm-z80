; RUN: not --crash llc -verify-machineinstrs -mtriple=z80 -O1 < %s 2>&1 | FileCheck %s

; z80_allreg has no stack fallback.  Six i16 arguments exceed HL, DE, BC,
; IX, and IY, so lowering must reject the call rather than silently selecting
; a stack ABI for the sixth argument.
define cc 129 i16 @too_many_i16(i16 %a, i16 %b, i16 %c, i16 %d, i16 %e,
                                i16 %f) {
  %r = add i16 %a, %f
  ret i16 %r
}

; CHECK: unable to lower arguments
