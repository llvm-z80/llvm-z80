; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl __neg_de
	.globl __neg_bc
	.globl ___divhi3
	.globl ___divhi3_pos_dividend
	.globl ___divhi3_pos_divisor

;===------------------------------------------------------------------------===;
__neg_de:
	xor	a
	sub	e
	ld	e, a
	sbc	a, a
	sub	d
	ld	d, a
	ret

;===------------------------------------------------------------------------===;
; Negate BC (helper, not exported). 6 bytes, 24 cycles.
;===------------------------------------------------------------------------===;
__neg_bc:
	xor	a
	sub	c
	ld	c, a
	sbc	a, a
	sub	b
	ld	b, a
	ret

;===------------------------------------------------------------------------===;
; ___divhi3 - 16-bit signed division
;
; Input:  DE = dividend, BC = divisor
; Output: BC = quotient (truncated toward zero)
; Method: determine result sign, make operands positive, call ___udivhi3
;===------------------------------------------------------------------------===;
___divhi3:
	ld	a, d
	xor	b		; bit 7 = result sign (1 if negative)
	push	af		; save result sign
	; Make dividend positive
	bit	7, d
	jr	z, ___divhi3_pos_dividend
	call	__neg_de
___divhi3_pos_dividend:
	; Make divisor positive
	bit	7, b
	jr	z, ___divhi3_pos_divisor
	call	__neg_bc
___divhi3_pos_divisor:
	call	___udivhi3	; BC = |quotient|
	pop	af
	bit	7, a
	ret	z		; positive result
	jr	__neg_bc		; negate and return (tail call)

;===------------------------------------------------------------------------===;
; ___umodhi3 - 16-bit unsigned modulo
;
; Input:  DE = dividend, BC = divisor
; Output: BC = remainder
;===------------------------------------------------------------------------===;
