; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s
;
; cc133 = CallingConv::Z80_SmallCCallee = the z88dk `__smallc __z88dk_callee`
; convention: the COMPOSITION of two orthogonal axes --
;   * argument order from __smallc  (cc132): left-to-right push, last arg at IX+4
;   * stack cleanup   from __z88dk_callee (cc131): the CALLEE pops the args
; Neither cc131 (right-to-left + callee) nor cc132 (left-to-right + caller) alone
; produces this; cc133 is the composed value the backend decodes per-axis.
; The z88dk classic clib (<graphics.h> plot_callee/draw_callee/...) uses it.
; Constants: 0x1111=4369, 0x2222=8738, 0x3333=13107.

declare cc133 i16 @fsc(i16, i16, i16)
declare cc133 void @sink2(i16, i16)

%ByValPair = type { i16, i16 }

; Caller side proves BOTH axes at once:
;   order  = __smallc: push 1st, 2nd, 3rd (left-to-right)  -- unlike cc131
;   cleanup= callee:   NO pop / inc sp after the call      -- unlike cc132
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
; CHECK:       inc sp
; CHECK:       inc sp
; CHECK:       inc sp
; CHECK:       inc sp
; CHECK:       push bc
; CHECK-NEXT:  ret
define cc133 void @callee_void(i16 %a, i16 %b) {
  %scaled_a = mul i16 %a, 10
  %s = add i16 %scaled_a, %b
  store i16 %s, ptr inttoptr(i16 16384 to ptr)
  ret void
}

; Byval arguments follow the same left-to-right mirrored stack layout as
; scalar arguments.  The aggregate starts at the highest stack offset here.
; CHECK-LABEL: _callee_byval:
; CHECK:       ld hl,#2
; CHECK:       add hl,sp
; CHECK:       ld hl,#4
; CHECK:       add hl,sp
define cc133 void @callee_byval(ptr byval(%ByValPair) %p, i16 %x) {
  %v = load i16, ptr %p
  %r = add i16 %v, %x
  store i16 %r, ptr inttoptr(i16 16384 to ptr)
  ret void
}
