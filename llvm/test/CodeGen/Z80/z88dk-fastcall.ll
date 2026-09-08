; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s
;
; __z88dk_fastcall (cc 130 = CallingConv::Z80_Z88dkFastCall).  z88dk's classic
; clib passes a SINGLE argument in a fixed register chosen by width, and
; returns in that same register:
;
;   width | argument & return register
;   ------+----------------------------
;   i8    | A
;   i16   | DE
;   i32   | HLDE  (HL = high word, DE = low word)

; ============================================================================
; (a) exact pattern, caller loads the single argument into the fixed register
; ============================================================================

declare cc 130 void @sink8(i8)
declare cc 130 void @sink32(i32)

; i8 argument in A.
define void @call_i8() {
; CHECK-LABEL: _call_i8:
; CHECK:      ld a,#17
; CHECK-NEXT: call _sink8
  call cc 130 void @sink8(i8 17)
  ret void
}

; i32 argument 0x11223344 in HLDE: HL = high word 0x1122 (4386),
; DE = low word 0x3344 (13124).
define void @call_i32() {
; CHECK-LABEL: _call_i32:
; CHECK-DAG:  ld hl,#4386
; CHECK-DAG:  ld de,#13124
  call cc 130 void @sink32(i32 287454020)
  ret void
}

; ============================================================================
; (b) structural variation, return value in the same fixed register
; ============================================================================

; i8 return in A.
define cc 130 i8 @ret_i8() {
; CHECK-LABEL: _ret_i8:
; CHECK:      ld a,#42
; CHECK-NEXT: ret
  ret i8 42
}

; i16 return in DE.
define cc 130 i16 @ret_i16() {
; CHECK-LABEL: _ret_i16:
; CHECK:      ld de,#4386
; CHECK-NEXT: ret
  ret i16 4386
}

; i32 return in HLDE: HL = high 0x1122 (4386), DE = low 0x3344 (13124).
define cc 130 i32 @ret_i32() {
; CHECK-LABEL: _ret_i32:
; CHECK-DAG:  ld hl,#4386
; CHECK-DAG:  ld de,#13124
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
