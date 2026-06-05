; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___divhi3
	.globl ___divhi3_pos_dividend
	.globl ___divhi3_pos_divisor

;===------------------------------------------------------------------------===;
; ___divhi3 - 16-bit signed division
;
; Input:  HL = dividend, DE = divisor
; Output: DE = quotient (truncated toward zero)
; Method: determine result sign, make operands positive, call ___udivhi3
;===------------------------------------------------------------------------===;
___divhi3:
	ld	a, h
	xor	d		; bit 7 = result sign (1 if negative)
	push	af		; save result sign
	; Make dividend positive
	bit	7, h
	jr	z, ___divhi3_pos_dividend
	xor	a
	sub	l
	ld	l, a
	sbc	a, a
	sub	h
	ld	h, a
___divhi3_pos_dividend:
	; Make divisor positive
	bit	7, d
	jr	z, ___divhi3_pos_divisor
	xor	a
	sub	e
	ld	e, a
	sbc	a, a
	sub	d
	ld	d, a
___divhi3_pos_divisor:
	call	___udivhi3	; DE = |quotient|
	pop	af
	bit	7, a
	ret	z		; positive result
	; Negate quotient
	xor	a
	sub	e
	ld	e, a
	sbc	a, a
	sub	d
	ld	d, a
	ret
