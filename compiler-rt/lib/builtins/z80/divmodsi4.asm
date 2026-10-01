; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___divmodsi4

;===------------------------------------------------------------------------===;
; ___divmodsi4 - 32-bit signed quotient and remainder
;
; Input:  HLDE = dividend, stack = divisor, then a pointer to the remainder
; Output: HLDE = quotient, *pointer = remainder; the caller pops the arguments
;
; Separate from divmodsi3.asm so that plain division does not link it.
;===------------------------------------------------------------------------===;
___divmodsi4:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	a, h
	push	af		; -1(ix): remainder sign
	xor	7(ix)
	push	af		; -3(ix): quotient sign
	call	__divmodsi_abs
	call	__udivmodsi	; HLDE = |quotient|, IY:BC = |remainder|
	push	hl
	push	de
	push	iy
	pop	hl
	ld	d, b
	ld	e, c
	bit	7, -1(ix)
	call	nz, __divmodsi_neg
	ld	c, 8(ix)
	ld	b, 9(ix)
	ld	a, e
	ld	(bc), a
	inc	bc
	ld	a, d
	ld	(bc), a
	inc	bc
	ld	a, l
	ld	(bc), a
	inc	bc
	ld	a, h
	ld	(bc), a
	pop	de
	pop	hl
	bit	7, -3(ix)
	call	nz, __divmodsi_neg
	ld	sp, ix
	pop	ix
	ret
