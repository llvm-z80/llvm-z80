; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___divmodhi4

;===------------------------------------------------------------------------===;
; ___divmodhi4 - 16-bit signed division with remainder (SM83)
;
; Input:  DE = dividend, BC = divisor
; Output: BC = quotient (truncated toward zero)
;         HL = remainder (sign of the dividend)
; Method: determine both signs, make operands positive, call ___udivmodhi4
;
; A quotient and a remainder of the same operands are fused into one call to
; this routine.
;===------------------------------------------------------------------------===;
___divmodhi4:
	ld	a, d
	xor	b		; bit 7 = quotient sign (1 if negative)
	bit	7, d		; Z = dividend non-negative
	push	af		; save both
	jr	z, ___divmodhi4_pos_dividend
	; Make dividend positive
	call	__neg_de
___divmodhi4_pos_dividend:
	; Make divisor positive
	bit	7, b
	jr	z, ___divmodhi4_pos_divisor
	call	__neg_bc
___divmodhi4_pos_divisor:
	call	___udivmodhi4	; BC = |quotient|, HL = |remainder|
	pop	af
	jr	z, ___divmodhi4_rem_done
	; Negate remainder, keeping the quotient sign in E
	ld	e, a
	xor	a
	sub	l
	ld	l, a
	sbc	a, a
	sub	h
	ld	h, a
	ld	a, e
___divmodhi4_rem_done:
	bit	7, a
	ret	z		; positive quotient
	jp	__neg_bc		; negate and return (tail call)
