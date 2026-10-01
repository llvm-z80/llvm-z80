; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___divmodsi4

;===------------------------------------------------------------------------===;
; ___divmodsi4 - 32-bit signed quotient and remainder
;
; Input:  DEBC = dividend, stack = divisor, then a pointer to the remainder
; Output: DEBC = quotient, *pointer = remainder
;
; Separate from divmodsi3.asm so that plain division does not link it. Both
; result signs ride in the core's sign slot.
;===------------------------------------------------------------------------===;
___divmodsi4:
	ldhl	sp, #5
	ld	a, (hl)		; divisor high byte
	xor	d
	and	#0x80		; bit 7: quotient sign
	bit	7, d
	jr	z, ___divmodsi4_signs
	inc	a		; bit 0: remainder sign
___divmodsi4_signs:
	push	af

	bit	7, d
	call	nz, __neg32_debc
	ldhl	sp, #7
	bit	7, (hl)
	jr	z, ___divmodsi4_div_pos
	ldhl	sp, #4
	call	__neg32_on_stack
___divmodsi4_div_pos:

	push	de
	push	bc
	ld	d, #0
	ld	e, d
	ld	b, d
	ld	c, d
	ld	a, #32
	push	af
	call	__udiv32_loop	; DEBC = |remainder|

	ldhl	sp, #7		; signs
	bit	0, (hl)
	call	nz, __neg32_debc
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
	pop	af		; signs
	bit	7, a
	call	nz, __neg32_debc
	pop	hl
	add	sp, #6
	jp	(hl)
