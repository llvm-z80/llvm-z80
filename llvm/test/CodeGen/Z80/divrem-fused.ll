; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O2 < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -mtriple=sm83 -z80-asm-format=sdasz80 -O2 < %s | FileCheck %s --check-prefix=SM83
;
; A quotient and a remainder of the same operands are fused into one runtime
; call. The ...hi3 routines promise only the quotient, so the fused call goes
; to the ...hi4 pair that names the remainder as a result too.

@q = global i16 0
@r = global i16 0

define void @sdivrem(i16 %a, i16 %b) {
; Z80-LABEL: _sdivrem:
; Z80:         call ___divmodhi4
; Z80-NEXT:    ld (_q),de
; Z80-NEXT:    ld (_r),hl
; SM83-LABEL: _sdivrem:
; SM83:        call ___divmodhi4
  %d = sdiv i16 %a, %b
  %m = srem i16 %a, %b
  store i16 %d, ptr @q
  store i16 %m, ptr @r
  ret void
}

define void @udivrem(i16 %a, i16 %b) {
; Z80-LABEL: _udivrem:
; Z80:         call ___udivmodhi4
; Z80-NEXT:    ld (_q),de
; Z80-NEXT:    ld (_r),hl
; SM83-LABEL: _udivrem:
; SM83:        call ___udivmodhi4
  %d = udiv i16 %a, %b
  %m = urem i16 %a, %b
  store i16 %d, ptr @q
  store i16 %m, ptr @r
  ret void
}

; A quotient alone keeps the ...hi3 routine.
define i16 @sdiv_only(i16 %a, i16 %b) {
; Z80-LABEL: _sdiv_only:
; Z80:         ___divhi3{{$}}
; SM83-LABEL: _sdiv_only:
; SM83:        ___divhi3{{$}}
  %d = sdiv i16 %a, %b
  ret i16 %d
}
