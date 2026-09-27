; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___umulhi3

;===------------------------------------------------------------------------===;
; ___umulhi3 - High half of the unsigned 16x16 product
;
; Input:  HL = a, DE = b
; Output: DE = (a * b) >> 16, HL = (a * b) & 0xFFFF (__mulsi3 uses both)
;
; MSB-first shift-and-add over DE:HL: the multiplier in DE shifts out as the
; product shifts in. A multiplier below 256 takes 8 steps.
;===------------------------------------------------------------------------===;
___umulhi3:
	ld	b, h
	ld	c, l
	ld	hl, #0
	ld	a, d
	or	a
	ld	a, #16
	jr	nz, ___umulhi3_loop
	ld	d, e
	ld	e, h
	ld	a, #8
___umulhi3_loop:
	add	hl, hl
	rl	e
	rl	d
	jr	nc, ___umulhi3_next
	add	hl, bc
	jr	nc, ___umulhi3_next
	inc	de
___umulhi3_next:
	dec	a
	jr	nz, ___umulhi3_loop
	ret
