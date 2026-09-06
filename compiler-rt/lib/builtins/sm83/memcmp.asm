; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memcmp

;===------------------------------------------------------------------------===;
; _memcmp - Compare memory blocks
;
; Input:  DE = ptr1, BC = ptr2, stack = size (i16)
; Output: BC = negative/zero/positive
; Uses SUB (HL) for comparing bytes directly from memory.
;===------------------------------------------------------------------------===;

_memcmp:
	; Load size: [ret_addr(2), size_lo, size_hi]
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = size
	; Rearrange: DE=ptr1, HL=ptr2, BC=size
	push	hl		; save size
	ld	h, b
	ld	l, c		; HL = ptr2
	pop	bc		; BC = size
_memcmp_loop:
	ld	a, b
	or	c
	jr	z, _memcmp_eq
	ld	a, (de)		; A = *ptr1
	sub	(hl)		; A = *ptr1 - *ptr2
	jr	nz, _memcmp_done
	inc	de
	inc	hl
	dec	bc
	jr	_memcmp_loop
_memcmp_eq:
	xor	a
_memcmp_done:
	ld	c, a		; sign-extend A into BC
	ld	b, #0
	bit	7, a
	jr	z, _memcmp_ret
	ld	b, #0xFF
_memcmp_ret:
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip 2 bytes of stack args
	jp	(hl)
