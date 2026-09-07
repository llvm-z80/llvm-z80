; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s
;
; cc133 = CallingConv::Z80_SmallCCallee = the z88dk `__smallc __z88dk_callee`
; convention: the COMPOSITION of two orthogonal axes.
;   * argument order from __smallc  (cc129): left-to-right push, last arg at IX+4
;   * stack cleanup   from __z88dk_callee (cc132): the CALLEE pops the args
; Neither cc132 (right-to-left + callee) nor cc129 (left-to-right + caller) alone
; produces this; cc133 is the value carrying both bits, which the backend
; decodes per-axis rather than special-casing.
; The z88dk classic clib (<graphics.h> plot_callee/draw_callee/...) uses it.
; Constants: 0x1111=4369, 0x2222=8738, 0x3333=13107.

declare cc133 i16 @fsc(i16, i16, i16)
declare cc133 void @sink2(i16, i16)

%ByValPair = type { i16, i16 }

; Caller side proves BOTH axes at once:
;   order  = __smallc: push 1st, 2nd, 3rd (left-to-right) , unlike cc132
;   cleanup= callee:   NO pop / inc sp after the call     , unlike cc129
; CHECK-LABEL: _call_smallc_callee:
; CHECK:       ld hl,#4369
; CHECK:       push hl
; CHECK:       ld hl,#8738
; CHECK:       push hl
; CHECK:       ld hl,#13107
; CHECK:       push hl
; CHECK:       call _fsc
; CHECK-NOT:   pop
; CHECK-NOT:   inc sp
; CHECK:       ret
define void @call_smallc_callee() {
  call cc133 i16 @fsc(i16 4369, i16 8738, i16 13107)
  ret void
}

; Callee side: void return -> callee cleans the four argument bytes while
; preserving the return address in BC.
; CHECK-LABEL: _callee_void:
; The first load is a+4 (the first, deepest argument), and the second is
; b+2 (the last argument nearest the return address).  The non-commutative
; result 10*a+b makes an accidental swap observable.
; CHECK:       ld hl,#4
; CHECK-NEXT:  add hl,sp
; CHECK-NEXT:  ld e,(hl)
; CHECK-NEXT:  inc hl
; CHECK-NEXT:  ld d,(hl)
; CHECK:       ld hl,#2
; CHECK-NEXT:  add hl,sp
; CHECK-NEXT:  ld c,(hl)
; CHECK-NEXT:  inc hl
; CHECK-NEXT:  ld b,(hl)
; CHECK:       add hl,bc
; CHECK:       pop bc
; CHECK-NEXT:  inc sp
; CHECK-NEXT:  inc sp
; CHECK-NEXT:  inc sp
; CHECK-NEXT:  inc sp
; CHECK-NEXT:  push bc
; CHECK-NEXT:  ret
define cc133 void @callee_void(i16 %a, i16 %b) {
  %scaled_a = mul i16 %a, 10
  %s = add i16 %scaled_a, %b
  store i16 %s, ptr inttoptr(i16 16384 to ptr)
  ret void
}

; Byval arguments follow the same left-to-right mirrored stack layout as
; scalar arguments: the aggregate is declared first so it sits deepest, at
; SP+4 through SP+7, leaving the trailing scalar at SP+2.  The result is
; 10*aggregate + scalar so that assigning the two slots the other way round
; is observable, and the callee pops all six bytes.
; CHECK-LABEL: _callee_byval:
; CHECK:       ld hl,#2
; CHECK-NEXT:  add hl,sp
; CHECK-NEXT:  ld c,(hl)
; CHECK-NEXT:  inc hl
; CHECK-NEXT:  ld b,(hl)
; CHECK-NEXT:  ld hl,#4
; CHECK-NEXT:  add hl,sp
; CHECK-NEXT:  ld e,(hl)
; CHECK-NEXT:  inc hl
; CHECK-NEXT:  ld d,(hl)
; The scaled operand is the one taken from SP+4, so a swapped layout changes
; the result rather than leaving it alone.
; CHECK-NEXT:  ld l,e
; CHECK-NEXT:  ld h,d
; CHECK-NEXT:  add hl,hl
; CHECK-NEXT:  add hl,hl
; CHECK-NEXT:  add hl,de
; CHECK-NEXT:  add hl,hl
; CHECK-NEXT:  add hl,bc
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
  %scaled = mul i16 %v, 10
  %r = add i16 %scaled, %x
  store i16 %r, ptr inttoptr(i16 16384 to ptr)
  ret void
}
