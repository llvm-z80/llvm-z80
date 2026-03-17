; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___umulhi3

;===------------------------------------------------------------------------===;
; ___umulhi3 - Upper 16 bits of 16-bit multiply (for 32-bit multiply narrowing)
;
; Input:  HL = multiplicand, DE = multiplier
; Output: DE = upper 16 bits of (HL * DE)
; Algorithm: shift-and-add with 32-bit accumulator using shadow registers
;   Main bank:   BC = multiplicand_lo, HL = accum_lo, DE = multiplier
;   Shadow bank: BC'= multiplicand_hi, HL'= accum_hi
;   When multiplier high byte is 0, only 8 iterations needed.
;===------------------------------------------------------------------------===;
___umulhi3:
	ld	b, h
	ld	c, l		; BC = multiplicand (lo)
	ld	hl, #0		; accum_lo = 0
	exx
	ld	hl, #0		; accum_hi = 0
	ld	b, #0
	ld	c, #0		; multiplicand_hi = 0
	exx
	ld	a, d
	or	a		; test multiplier high byte
	ld	a, #16
	jr	nz, ___umulhi3_loop
	ld	a, #8		; high byte is 0, only 8 iterations
___umulhi3_loop:
	srl	d		; multiplier >>= 1, LSB -> carry
	rr	e
	jr	nc, ___umulhi3_skip
	add	hl, bc		; accum_lo += multiplicand_lo
	exx
	adc	hl, bc		; accum_hi += multiplicand_hi + carry
	exx
___umulhi3_skip:
	sla	c		; multiplicand_lo <<= 1
	rl	b		; carry out from lo
	exx
	rl	c		; carry into multiplicand_hi
	rl	b
	exx
	dec	a
	jr	nz, ___umulhi3_loop
	; result high is in HL' (shadow)
	exx
	push	hl		; push accum_hi
	exx
	pop	de		; DE = upper 16 bits
	ret
