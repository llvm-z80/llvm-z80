; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___mulhi3

;===------------------------------------------------------------------------===;
; ___mulhi3 - 16-bit unsigned/signed multiply
;
; Input:  HL = multiplicand, DE = multiplier
; Output: DE = HL * DE (low 16 bits)
; Algorithm: MSB-first shift-and-add with 8-bit fast path
;   When multiplier high byte is 0, only 8 iterations needed.
;   Based on SDCC's __mul16 by Michael Hope / Philipp Klaus Krause.
;===------------------------------------------------------------------------===;
___mulhi3:
	ld	b, d
	ld	c, e		; BC = multiplier (will be shifted left)
	ex	de, hl		; DE = multiplicand (will be added to result)
	xor	a
	ld	h, a
	ld	l, a		; HL = 0 (result accumulator)
	or	b		; test multiplier high byte
	ld	b, #16		; assume 16 iterations
	jr	nz, ___mulhi3_full
	ld	b, #8		; high byte is 0, only 8 iterations needed
	ld	a, c		; A = multiplier low byte
___mulhi3_loop:
	add	hl, hl		; result <<= 1
___mulhi3_full:
	rl	c		; shift multiplier left, MSB -> carry
	rla			; shift high byte left
	jr	nc, ___mulhi3_skip
	add	hl, de		; result += multiplicand
___mulhi3_skip:
	djnz	___mulhi3_loop
	ex	de, hl		; DE = result
	ret
