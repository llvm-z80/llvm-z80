; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memcmp
	.globl _memcmp_loop
	.globl _memcmp_eq
	.globl _memcmp_done
	.globl _memcmp_ret

_memcmp:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, 4(ix)	; BC = size
	ld	b, 5(ix)
_memcmp_loop:
	ld	a, b
	or	c
	jr	z, _memcmp_eq
	ld	a, (de)
	ld	4(ix), a	; temp = *ptr2
	ld	a, (hl)
	sub	4(ix)		; A = *ptr1 - *ptr2
	jr	nz, _memcmp_done
	inc	hl
	inc	de
	dec	bc
	jr	_memcmp_loop
_memcmp_eq:
	xor	a
_memcmp_done:
	ld	e, a
	ld	d, #0
	bit	7, a
	jr	z, _memcmp_ret
	ld	d, #0xFF
_memcmp_ret:
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc
	ret

;===------------------------------------------------------------------------===;
; _memchr - Find byte in memory block
;
; Input:  HL = ptr, DE = byte (E = value), stack = size (i16)
; Output: DE = pointer to byte, or 0 if not found
