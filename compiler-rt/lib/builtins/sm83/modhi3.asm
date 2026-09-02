; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___modhi3

;===------------------------------------------------------------------------===;
; ___modhi3 - 16-bit signed modulo
;
; Input:  DE = dividend, BC = divisor
; Output: BC = remainder (same sign as dividend, C99)
;===------------------------------------------------------------------------===;

___modhi3:
	ld	a, d		; save dividend sign
	push	af
	; Make dividend positive
	bit	7, d
	jr	z, ___modhi3_pos_dividend
	call	__neg_de
___modhi3_pos_dividend:
	; Make divisor positive
	bit	7, b
	jr	z, ___modhi3_pos_divisor
	call	__neg_bc
___modhi3_pos_divisor:
	call	___udivhi3	; BC = quotient, HL = remainder
	ld	c, l		; BC = |remainder|
	ld	b, h
	pop	af
	bit	7, a
	ret	z		; dividend was positive
	jp	__neg_bc		; negate and return (tail call)
