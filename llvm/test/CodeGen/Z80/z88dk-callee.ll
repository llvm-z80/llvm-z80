; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s
;
; __z88dk_callee on its own (cc 131 = CallingConv::Z80_Z88dkCallee).  In SDCC
; the keyword is a MODIFIER over whichever __sdcccall level is in effect, not a
; convention of its own, so a bare __z88dk_callee keeps __sdcccall(1) argument
; passing and return registers and only moves stack cleanup to the callee.
; Spelling it __sdcccall(0) __z88dk_callee instead selects cc 132, where every
; argument goes on the stack, see sdcccall0-callee.ll.
;
; What makes cc 131 distinct from plain __sdcccall(1) is that cleanup is forced
; regardless of the return size; __sdcccall(1) hands it back to the caller once
; the return exceeds 16 bits.  Verified against SDCC 4.6.0: for a long return
; `__sdcccall(1)` ends `pop ix / ret` while `__sdcccall(1) __z88dk_callee` ends
; `pop ix / pop iy / pop af / jp (iy)`.

declare cc 131 void @sink3(i16, i16, i16)
declare cc 131 void @sink4(i16, i16, i16, i16)

; Caller side: the first two arguments still travel in HL and DE and only the
; third is pushed, this is what separates cc 131 from cc 132.  The caller
; does NOT clean up.
; CHECK-LABEL: _call_callee:
; CHECK:      ld hl,#13107
; CHECK:      push hl
; CHECK-DAG:  ld hl,#4369
; CHECK-DAG:  ld de,#8738
; CHECK:      call _sink3
; CHECK-NOT:  pop
; CHECK-NOT:  inc sp
; CHECK:      ret
define void @call_callee() {
  call cc 131 void @sink3(i16 4369, i16 8738, i16 13107)
  ret void
}

; Caller side with 4 args: arg3 and arg4 overflow to the stack.
; Overflow args are pushed right-to-left (arg4 first/deepest, arg3 second).
; CHECK-LABEL: _call_callee4:
; CHECK:      ld hl,#17476
; CHECK-NEXT: push hl
; CHECK:      ld hl,#13107
; CHECK-NEXT: push hl
; CHECK-DAG:  ld hl,#4369
; CHECK-DAG:  ld de,#8738
; CHECK:      call _sink4
; CHECK-NOT:  pop
; CHECK-NOT:  inc sp
; CHECK:      ret
define void @call_callee4() {
  call cc 131 void @sink4(i16 4369, i16 8738, i16 13107, i16 17476)
  ret void
}

; Callee side with a 32-bit return: __sdcccall(1) alone would let the CALLER
; pop the two argument bytes here, but the modifier forces the callee to do it.
; Expression a-2b-c has coefficients +1,-2,-1 (all distinct), so any register
; or stack-slot swap produces a detectably wrong result.
; a is in HL, b is in DE, c is at SP+2 (arg3 stack overflow, shallower).
; CHECK-LABEL: _callee_reti32:
; CHECK:      ld c,l
; CHECK-NEXT: ld b,h
; CHECK:      add hl,hl
; CHECK:      ld hl,#4
; CHECK-NEXT: add hl,sp
; CHECK:      sbc hl,de
; CHECK:      pop bc
; CHECK-NEXT: inc sp
; CHECK-NEXT: inc sp
; CHECK-NEXT: push bc
; CHECK-NEXT: ret
define cc 131 i32 @callee_reti32(i16 %a, i16 %b, i16 %c) {
  %b2 = shl i16 %b, 1
  %ab = sub i16 %a, %b2
  %abc = sub i16 %ab, %c
  %z = sext i16 %abc to i32
  ret i32 %z
}

; A 16-bit return would be callee-cleaned under plain __sdcccall(1) too, so
; this one only pins that the modifier does not double-clean.
; CHECK-LABEL: _callee_reti16:
; CHECK:      ld c,l
; CHECK-NEXT: ld b,h
; CHECK:      add hl,hl
; CHECK:      ld hl,#4
; CHECK-NEXT: add hl,sp
; CHECK:      sbc hl,de
; CHECK:      ex de,hl
; CHECK-NEXT: pop bc
; CHECK-NEXT: inc sp
; CHECK-NEXT: inc sp
; CHECK-NEXT: push bc
; CHECK-NEXT: ret
define cc 131 i16 @callee_reti16(i16 %a, i16 %b, i16 %c) {
  %b2 = shl i16 %b, 1
  %ab = sub i16 %a, %b2
  %abc = sub i16 %ab, %c
  ret i16 %abc
}

; Four-argument callee: arg1=HL, arg2=DE, arg3=stack SP+2, arg4=stack SP+4.
; Expression a-2b-c+2d has coefficients +1,-2,-1,+2 (all distinct).
; Verifies that both stack overflow slots are read in the correct order.
; Callee pops all 4 overflow bytes.
; CHECK-LABEL: _callee_4arg:
; CHECK:      ld c,l
; CHECK-NEXT: ld b,h
; CHECK:      ld l,e
; CHECK-NEXT: ld h,d
; CHECK:      ld hl,#4
; CHECK-NEXT: add hl,sp
; CHECK:      sbc hl,de
; CHECK:      ld hl,#4
; CHECK-NEXT: add hl,sp
; CHECK:      add hl,hl
; CHECK:      add hl,de
; CHECK:      pop bc
; CHECK-NEXT: inc sp
; CHECK-NEXT: inc sp
; CHECK-NEXT: inc sp
; CHECK-NEXT: inc sp
; CHECK-NEXT: push bc
; CHECK-NEXT: ret
define cc 131 i16 @callee_4arg(i16 %a, i16 %b, i16 %c, i16 %d) {
  %b2 = shl i16 %b, 1
  %ab = sub i16 %a, %b2
  %abc = sub i16 %ab, %c
  %d2 = shl i16 %d, 1
  %r = add i16 %abc, %d2
  ret i16 %r
}

; No stack argument at all, nothing to clean, and no spurious pop.
; CHECK-LABEL: _callee_tworegs:
; CHECK-NOT:  pop
; CHECK-NOT:  inc sp
; CHECK:      ret
define cc 131 i16 @callee_tworegs(i16 %a, i16 %b) {
  %s = sub i16 %a, %b
  ret i16 %s
}
