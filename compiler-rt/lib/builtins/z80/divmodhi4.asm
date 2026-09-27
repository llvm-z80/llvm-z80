; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___divmodhi4

;===------------------------------------------------------------------------===;
; ___divmodhi4 - 16-bit signed division with remainder
;
; Input:  HL = dividend, DE = divisor
; Output: DE = quotient (truncated toward zero)
;         HL = remainder (sign of the dividend)
; Method: determine both signs, make operands positive, call ___udivmodhi4
;
; A quotient and a remainder of the same operands are fused into one call to
; this routine.
;===------------------------------------------------------------------------===;
___divmodhi4:
	ld	a, h
	xor	d		; bit 7 = quotient sign (1 if negative)
	bit	7, h		; Z = dividend non-negative
	push	af		; save both
	jr	z, ___divmodhi4_pos_dividend
	; Make dividend positive
	xor	a
	sub	l
	ld	l, a
	sbc	a, a
	sub	h
	ld	h, a
___divmodhi4_pos_dividend:
	; Make divisor positive
	bit	7, d
	jr	z, ___divmodhi4_pos_divisor
	xor	a
	sub	e
	ld	e, a
	sbc	a, a
	sub	d
	ld	d, a
___divmodhi4_pos_divisor:
	call	___udivmodhi4	; DE = |quotient|, HL = |remainder|
	pop	af
	jr	z, ___divmodhi4_rem_done
	; Negate remainder, keeping the quotient sign in A
	ld	b, h
	ld	c, l
	ld	hl, #0
	or	a
	sbc	hl, bc
___divmodhi4_rem_done:
	bit	7, a
	ret	z		; positive quotient
	; Negate quotient
	xor	a
	sub	e
	ld	e, a
	sbc	a, a
	sub	d
	ld	d, a
	ret
