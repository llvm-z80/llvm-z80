; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___mulhi3

;===------------------------------------------------------------------------===;
; ___mulhi3 - 16-bit unsigned/signed multiply
;
; Input:  DE = multiplicand, BC = multiplier
; Output: BC = DE * BC (low 16 bits)
; Algorithm: shift-and-add, LSB first, with 8-bit fast path
;   When multiplier high byte is 0, only 8 iterations needed.
;
; Register usage during loop:
;   HL = result accumulator (ADD HL,BC for accumulation)
;   BC = multiplicand (shifted left each iteration)
;   DE = multiplier (shifted right each iteration)
;   A  = bit counter
;===------------------------------------------------------------------------===;
___mulhi3:
	; Swap DE<->BC: need BC=multiplicand, DE=multiplier
	push	bc		; save multiplier
	ld	b, d
	ld	c, e		; BC = multiplicand
	pop	de		; DE = multiplier
	ld	hl, #0		; result accumulator
	ld	a, d
	or	a		; test multiplier high byte
	ld	a, #16
	jr	nz, ___mulhi3_loop
	ld	a, #8		; high byte is 0, only 8 iterations
___mulhi3_loop:
	srl	d		; DE >>= 1, LSB -> carry
	rr	e
	jr	nc, ___mulhi3_skip
	add	hl, bc		; result += multiplicand
___mulhi3_skip:
	sla	c		; BC <<= 1 (multiplicand doubles)
	rl	b
	dec	a
	jr	nz, ___mulhi3_loop
	ld	c, l		; BC = result
	ld	b, h
	ret
