; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s
;
; Controls for __z88dk_fastcall (cc 130): the neighbouring conventions it must
; not disturb.

; ----------------------------------------------------------------------------
; The default C convention (cc 1) is what compiler-rt libcalls use.  It returns
; i16 in DE, NOT HL, the clang divide helpers in z88dk/libsrc/l/clang rely on
; the DE return and `ex de,hl` accordingly.
; ----------------------------------------------------------------------------
define i16 @def_ret_i16() {
; CHECK-LABEL: _def_ret_i16:
; CHECK:      ld de,#4386
; CHECK:      ret
  ret i16 4386
}

; The default C convention returns i8 in A.
define i8 @def_ret_i8() {
; CHECK-LABEL: _def_ret_i8:
; CHECK:      ld a,#42
; CHECK:      ret
  ret i8 42
}

; ----------------------------------------------------------------------------
; 16-bit coincidence: the default convention already passes a sole 16-bit
; argument in HL, and so does z88dk fastcall.  This is the one case that looks
; identical under both, and it must stay HL.
; ----------------------------------------------------------------------------
declare cc 130 void @fc_i16(i16)
define void @fc_i16_caller() {
; CHECK-LABEL: _fc_i16_caller:
; CHECK:      ld hl,#4386
  call cc 130 void @fc_i16(i16 4386)
  ret void
}
