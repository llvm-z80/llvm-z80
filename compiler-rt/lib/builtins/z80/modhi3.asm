; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___modhi3
	.globl ___modhi3_pos_dividend
	.globl ___modhi3_pos_divisor

;===------------------------------------------------------------------------===;
; ___modhi3 - 16-bit signed modulo
;
; Input:  HL = dividend, DE = divisor
; Output: DE = remainder (same sign as dividend, C99)
;===------------------------------------------------------------------------===;
___modhi3:
	ld	a, h		; save dividend sign
	push	af
	; Make dividend positive
	bit	7, h
	jr	z, ___modhi3_pos_dividend
	xor	a
	sub	l
	ld	l, a
	sbc	a, a
	sub	h
	ld	h, a
___modhi3_pos_dividend:
	; Make divisor positive
	bit	7, d
	jr	z, ___modhi3_pos_divisor
	xor	a
	sub	e
	ld	e, a
	sbc	a, a
	sub	d
	ld	d, a
___modhi3_pos_divisor:
	call	___udivhi3	; DE = quotient, HL = remainder
	ex	de, hl		; DE = remainder
	pop	af
	bit	7, a
	ret	z		; dividend was positive
	; Negate remainder
	xor	a
	sub	e
	ld	e, a
	sbc	a, a
	sub	d
	ld	d, a
	ret
