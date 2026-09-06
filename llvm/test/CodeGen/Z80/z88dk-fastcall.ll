; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s
;
; ravn/llvm-z80: __z88dk_fastcall calling convention (cc 130 =
; CallingConv::Z80_Z88dkFastCall).  z88dk's classic clib passes a SINGLE
; argument in a fixed register by width and returns in that same register:
;
;   width | argument & return register
;   ------+----------------------------
;   i8    | L
;   i16   | HL
;   i32   | DEHL  (DE = high word, HL = low word)
;
; Verified from z88dk source: libsrc/target/osca/rs232/rs232_put.asm reads its
; i8 argument with `ld a, l` (arg in L); libsrc/classic/stdlib/swapendian.asm
; takes/returns a void* in HL.  These registers are exactly the sdcccall(0)
; RETURN registers (Z80CallLowering.cpp CCRegs0: Ret_I8=L, Ret_I16=HL,
; Ret_I32=DE:HL); fastcall additionally passes the single argument in them.
;
; IMPLEMENTED (cc 130 = CallingConv::Z80_Z88dkFastCall): the CHECKs below encode
; the z88dk behaviour and now PASS.  This started life as an expected-failure
; test written before the backend support; the expected-failure directive was
; removed in the same changeset that wired up the convention (leaving it stale
; would make lit report XPASS and fail CI, the intended tripwire).

; ============================================================================
; (a) exact pattern -- caller loads the single argument into the fixed register
; ============================================================================

declare cc 130 void @sink8(i8)
declare cc 130 void @sink32(i32)

; i8 argument must be routed through L, never A.
; Arg routed through L: `ld l,#17`.
define void @call_i8() {
; CHECK-LABEL: _call_i8:
; CHECK:      ld l
  call cc 130 void @sink8(i8 17)
  ret void
}

; i32 argument 0x11223344 in DEHL: HL = low word 0x3344 (13124),
; DE = high word 0x1122 (4386).
; DE:HL order: `ld de,#4386` (high) / `ld hl,#13124` (low).
define void @call_i32() {
; CHECK-LABEL: _call_i32:
; CHECK-DAG:  ld hl,#13124
; CHECK-DAG:  ld de,#4386
  call cc 130 void @sink32(i32 287454020)
  ret void
}

; ============================================================================
; (b) structural variation -- return value in the same fixed register
; ============================================================================

; i8 return in L (not A): `ld l,#42`.
define cc 130 i8 @ret_i8() {
; CHECK-LABEL: _ret_i8:
; CHECK:      ld l,#42
; CHECK-NEXT: ret
  ret i8 42
}

; i16 return in HL (not DE): `ld hl,#4386`.
define cc 130 i16 @ret_i16() {
; CHECK-LABEL: _ret_i16:
; CHECK:      ld hl,#4386
; CHECK-NEXT: ret
  ret i16 4386
}

; i32 return in DEHL: DE = high 0x1122 (4386), HL = low 0x3344 (13124).
; i32 return DE:HL: `ld hl,#13124` (low) / `ld de,#4386` (high).
define cc 130 i32 @ret_i32() {
; CHECK-LABEL: _ret_i32:
; CHECK-DAG:  ld hl,#13124
; CHECK-DAG:  ld de,#4386
; CHECK:      ret
  ret i32 287454020
}

; ============================================================================
; (d) boundary -- a void fastcall function is just a plain call/ret
; ============================================================================

define cc 130 void @ret_void() {
; CHECK-LABEL: _ret_void:
; CHECK: ret{{$}}
  ret void
}
