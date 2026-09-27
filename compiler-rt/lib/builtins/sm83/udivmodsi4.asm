; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___udivmodsi4

;===------------------------------------------------------------------------===;
; ___udivmodsi4 - Unsigned 32-bit quotient and remainder
;
; Input:  DEBC = dividend, stack = divisor, then a pointer to the remainder
; Output: DEBC = quotient, *pointer = remainder
;
; Separate from divmodsi3.asm so that plain division does not link it.
;===------------------------------------------------------------------------===;
___udivmodsi4:
	push	af		; dummy
	push	de
	push	bc
	ld	d, #0
	ld	e, d
	ld	b, d
	ld	c, d
	ld	a, #32
	push	af
	call	__udiv32_loop
	ldhl	sp, #14		; remainder pointer
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	a, c
	ld	(hl+), a
	ld	a, b
	ld	(hl+), a
	ld	a, e
	ld	(hl+), a
	ld	a, d
	ld	(hl), a
	pop	af
	pop	bc
	pop	de
	pop	af
	pop	hl
	add	sp, #6
	jp	(hl)
