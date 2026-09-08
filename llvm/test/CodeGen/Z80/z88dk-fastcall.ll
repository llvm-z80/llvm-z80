; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s
;
; __z88dk_fastcall (cc 130 = CallingConv::Z80_Z88dkFastCall).  z88dk's classic
; clib passes a SINGLE argument in a fixed register chosen by width, and
; returns in that same register, always a subset of DEHL:
;
;   width | argument & return register
;   ------+----------------------------
;   i8    | L
;   i16   | HL
;   i32   | DEHL  (DE = high word, HL = low word)
;
; Verified from z88dk source: libsrc/target/osca/rs232/rs232_put.asm reads its
; i8 argument with `ld a, l` (arg in L); libsrc/classic/stdlib/swapendian.asm
; takes/returns a void* in HL.  These are exactly the __sdcccall(0) RETURN
; registers; fastcall additionally passes its sole argument in them.

; ============================================================================
; (a) exact pattern, caller loads the single argument into the fixed register
; ============================================================================

declare cc 130 void @sink8(i8)
declare cc 130 void @sink32(i32)

; i8 argument in L (never A).
define void @call_i8() {
; CHECK-LABEL: _call_i8:
; CHECK-NOT:  ld a,#17
; CHECK:      ld l,#17
; CHECK-NEXT: call _sink8
  call cc 130 void @sink8(i8 17)
  ret void
}

; i32 argument 0x11223344 in DEHL: DE = high word 0x1122 (4386),
; HL = low word 0x3344 (13124).
define void @call_i32() {
; CHECK-LABEL: _call_i32:
; CHECK-DAG:  ld de,#4386
; CHECK-DAG:  ld hl,#13124
  call cc 130 void @sink32(i32 287454020)
  ret void
}

; ============================================================================
; (b) structural variation, return value in the same fixed register
; ============================================================================

; i8 return in L (not A).
define cc 130 i8 @ret_i8() {
; CHECK-LABEL: _ret_i8:
; CHECK:      ld l,#42
; CHECK-NEXT: ret
  ret i8 42
}

; i16 return in HL (not DE).
define cc 130 i16 @ret_i16() {
; CHECK-LABEL: _ret_i16:
; CHECK:      ld hl,#4386
; CHECK-NEXT: ret
  ret i16 4386
}

; i32 return in DEHL: DE = high 0x1122 (4386), HL = low 0x3344 (13124).
define cc 130 i32 @ret_i32() {
; CHECK-LABEL: _ret_i32:
; CHECK-DAG:  ld de,#4386
; CHECK-DAG:  ld hl,#13124
; CHECK:      ret
  ret i32 287454020
}

; ============================================================================
; (c) boundary, a void fastcall function is just a plain call/ret
; ============================================================================

define cc 130 void @ret_void() {
; CHECK-LABEL: _ret_void:
; CHECK-NOT:  pop
; CHECK-NOT:  inc sp
; CHECK:      ret{{$}}
  ret void
}
