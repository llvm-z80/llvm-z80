; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___udivmodsi4

;===------------------------------------------------------------------------===;
; ___udivmodsi4 - 32-bit unsigned quotient and remainder
;
; Input:  HLDE = dividend, stack = divisor, then a pointer to the remainder
; Output: HLDE = quotient, *pointer = remainder; the caller pops the arguments
;
; Separate from divmodsi3.asm so that plain division does not link it.
;===------------------------------------------------------------------------===;
___udivmodsi4:
	push	ix
	ld	ix, #0
	add	ix, sp
	call	__udivmodsi
	push	hl
	ld	l, 8(ix)
	ld	h, 9(ix)
	ld	(hl), c
	inc	hl
	ld	(hl), b
	inc	hl
	push	iy
	pop	bc
	ld	(hl), c
	inc	hl
	ld	(hl), b
	pop	hl
	pop	ix
	ret
