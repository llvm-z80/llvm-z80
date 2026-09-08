; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s
;
; cc133 = CallingConv::Z80_SmallCCallee = the z88dk `__smallc __z88dk_callee`
; convention: the COMPOSITION of two orthogonal axes.
;   * arg passing from __smallc (cc129): first two args in HL/DE, overflow pushed
;   * stack cleanup from __z88dk_callee: the CALLEE pops the overflow args
; The z88dk classic clib (<graphics.h> plot_callee/draw_callee/...) uses it.
; Constants: 0x1111=4369, 0x2222=8738, 0x3333=13107.

declare cc133 i16 @fsc(i16, i16, i16)
declare cc133 void @sink2(i16, i16)

%ByValPair = type { i16, i16 }

; Caller side proves BOTH axes at once:
;   passing  = __smallc: arg1 in HL, arg2 in DE, arg3 pushed (overflow)
;   cleanup  = callee:   NO pop / inc sp after the call
; CHECK-LABEL: _call_smallc_callee:
; CHECK:       ld hl,#13107
; CHECK-NEXT:  push hl
; CHECK-DAG:   ld hl,#4369
; CHECK-DAG:   ld de,#8738
; CHECK:       call _fsc
; CHECK-NOT:   pop
; CHECK-NOT:   inc sp
; CHECK:       ret
define void @call_smallc_callee() {
  call cc133 i16 @fsc(i16 4369, i16 8738, i16 13107)
  ret void
}

; Callee side: both args fit in HL/DE, nothing on the stack, so no cleanup.
; The non-commutative result 10*a+b makes an accidental swap observable.
; CHECK-LABEL: _callee_void:
; CHECK-NOT:   add hl,sp
; CHECK-NOT:   pop
; CHECK-NOT:   inc sp
; CHECK:       ret
define cc133 void @callee_void(i16 %a, i16 %b) {
  %scaled_a = mul i16 %a, 10
  %s = add i16 %scaled_a, %b
  store i16 %s, ptr inttoptr(i16 16384 to ptr)
  ret void
}

; Byval cannot go in registers, so both args land on the stack.  The scalar
; is deepest at SP+6, the byval first i16 is at SP+2.  The result is
; 10*byval+scalar so that swapping the two slots is observable.
; Callee pops all six bytes.
; CHECK-LABEL: _callee_byval:
; CHECK:       ld hl,#6
; CHECK-NEXT:  add hl,sp
; CHECK:       ld hl,#2
; CHECK-NEXT:  add hl,sp
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
