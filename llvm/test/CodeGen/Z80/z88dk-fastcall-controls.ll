; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s
;
; Positive controls for the __z88dk_fastcall work (planned cc 130).  These pin
; behaviour that must stay UNCHANGED when fastcall is implemented -- they guard
; against collateral damage to the neighbouring conventions.  NOT XFAIL: they
; pass today and must keep passing after cc 130 is wired up.

; ----------------------------------------------------------------------------
; The default C convention (cc 1) is what compiler-rt libcalls use.  It returns
; i16 in DE (NOT HL).  This must not change -- the clang divide helpers in
; z88dk/libsrc/l/clang rely on the DE return and `ex de,hl` accordingly.
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
; 16-bit coincidence: an i16 fastcall argument already lands in HL today
; (default passes the sole 16-bit arg in HL, and z88dk fastcall also uses HL).
; This is the one case that works before AND after, and must remain HL.
; ----------------------------------------------------------------------------
declare cc 130 void @fc_i16(i16)
define void @fc_i16_caller() {
; CHECK-LABEL: _fc_i16_caller:
; CHECK:      ld hl,#4386
  call cc 130 void @fc_i16(i16 4386)
  ret void
}
