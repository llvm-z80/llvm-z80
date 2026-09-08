; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s
;
; cc133 = CallingConv::Z80_SmallCCallee = the z88dk `__smallc __z88dk_callee`
; convention: the COMPOSITION of two orthogonal axes.
;   * arg passing from __smallc (cc129): all args pushed left-to-right
;     (first arg deepest, last arg shallowest at SP+2 after ret-addr)
;   * stack cleanup from __z88dk_callee: the CALLEE pops the stack args
; The z88dk classic clib (<graphics.h> plot_callee/draw_callee/...) uses it.
; Constants: 0x1111=4369, 0x2222=8738, 0x3333=13107.

declare cc133 i16 @fsc(i16, i16, i16)
declare cc133 void @sink2(i16, i16)

%ByValPair = type { i16, i16 }

; Caller side proves BOTH axes at once:
;   passing  = __smallc: all args pushed left-to-right (4369 first, 13107 last)
;   cleanup  = callee:   NO pop / inc sp after the call
; CHECK-LABEL: _call_smallc_callee:
; CHECK:       ld hl,#4369
; CHECK-NEXT:  push hl
; CHECK:       ld hl,#8738
; CHECK-NEXT:  push hl
; CHECK:       ld hl,#13107
; CHECK-NEXT:  push hl
; CHECK:       call _fsc
; CHECK-NOT:   pop
; CHECK-NOT:   inc sp
; CHECK:       ret
define void @call_smallc_callee() {
  call cc133 i16 @fsc(i16 4369, i16 8738, i16 13107)
  ret void
}

; Callee side: arg1 (a) is deepest at SP+4, arg2 (b) shallowest at SP+2.
; The result a-2b gives the two operands distinct coefficients, so a swapped
; layout changes the value rather than leaving it alone, and the doubling is
; visibly applied to whichever slot b came from.  The callee pops all 4 bytes.
; CHECK-LABEL: _callee_void:
; CHECK:       ld hl,#4
; CHECK-NEXT:  add hl,sp
; CHECK-NEXT:  ld c,(hl)
; CHECK-NEXT:  inc hl
; CHECK-NEXT:  ld b,(hl)
; CHECK-NEXT:  ld hl,#2
; CHECK-NEXT:  add hl,sp
; CHECK-NEXT:  ld e,(hl)
; CHECK-NEXT:  inc hl
; CHECK-NEXT:  ld d,(hl)
; b is the doubled operand.
; CHECK-NEXT:  ld l,e
; CHECK-NEXT:  ld h,d
; CHECK-NEXT:  add hl,hl
; CHECK:       sbc hl,de
; CHECK:       pop bc
; CHECK-NEXT:  inc sp
; CHECK-NEXT:  inc sp
; CHECK-NEXT:  inc sp
; CHECK-NEXT:  inc sp
; CHECK-NEXT:  push bc
; CHECK-NEXT:  ret
define cc133 void @callee_void(i16 %a, i16 %b) {
  %b2 = shl i16 %b, 1
  %s = sub i16 %a, %b2
  store i16 %s, ptr inttoptr(i16 16384 to ptr)
  ret void
}

; Byval cannot go in registers, so both arguments land on the stack.  Declared
; first, the aggregate sits deepest at SP+4 through SP+7, leaving the trailing
; scalar at SP+2.  The scalar is read first here, and the aggregate afterwards
; through SP+6 because the push in between moved SP.  The callee pops all 6
; bytes.
; CHECK-LABEL: _callee_byval:
; CHECK:       ld hl,#2
; CHECK-NEXT:  add hl,sp
; CHECK-NEXT:  ld c,(hl)
; CHECK-NEXT:  inc hl
; CHECK-NEXT:  ld b,(hl)
; CHECK-NEXT:  ld l,c
; CHECK-NEXT:  ld h,b
; CHECK-NEXT:  push hl
; CHECK-NEXT:  ld hl,#6
; CHECK-NEXT:  add hl,sp
; The scalar is the doubled operand, so swapping the two slots is observable.
; CHECK:       add hl,hl
; CHECK:       sbc hl,de
; CHECK:       pop bc
; CHECK-NEXT:  inc sp
; CHECK-NEXT:  inc sp
; CHECK-NEXT:  inc sp
; CHECK-NEXT:  inc sp
; CHECK-NEXT:  inc sp
; CHECK-NEXT:  inc sp
; CHECK-NEXT:  push bc
; CHECK-NEXT:  ret
define cc133 void @callee_byval(ptr byval(%ByValPair) %p, i16 %x) {
  %v = load i16, ptr %p
  %x2 = shl i16 %x, 1
  %r = sub i16 %v, %x2
  store i16 %r, ptr inttoptr(i16 16384 to ptr)
  ret void
}
