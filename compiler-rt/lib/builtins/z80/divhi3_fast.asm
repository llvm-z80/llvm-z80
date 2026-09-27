; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___divhi3_fast

;===------------------------------------------------------------------------===;
; ___divhi3_fast - 16-bit signed division, speed variant (ravn/llvm-z80 #244)
;
; Identical to ___divhi3 but calls the fully-unrolled ___udivhi3_fast.  Selected
; by the backend only at -O3 (CodeGenOptLevel::Aggressive) on Z80.
;
; Input:  HL = dividend, DE = divisor
; Output: DE = quotient (truncated toward zero)
;===------------------------------------------------------------------------===;
___divhi3_fast:
	ld	a, h
	xor	d		; bit 7 = result sign
	push	af
	bit	7, h
	jr	z, .Lpos_num
	xor	a
	sub	l
	ld	l, a
	sbc	a, a
	sub	h
	ld	h, a
.Lpos_num:
	bit	7, d
	jr	z, .Lpos_den
	xor	a
	sub	e
	ld	e, a
	sbc	a, a
	sub	d
	ld	d, a
.Lpos_den:
	call	___udivhi3_fast	; DE = |quotient|
	pop	af
	bit	7, a
	ret	z
	xor	a
	sub	e
	ld	e, a
	sbc	a, a
	sub	d
	ld	d, a
	ret
