; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strncmp

;===------------------------------------------------------------------------===;
; _strncmp - Compare two strings up to n bytes
;
; Input:  DE = str1, BC = str2, stack = n (i16)
; Output: BC = negative/zero/positive
; Uses SUB (HL) for comparing bytes directly from memory.
;===------------------------------------------------------------------------===;

_strncmp:
	; Load n from stack: [ret_addr(2), n_lo, n_hi]
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = n
	; Rearrange: DE=str1, HL=str2, BC=n
	push	hl		; save n
	ld	h, b
	ld	l, c		; HL = str2
	pop	bc		; BC = n
_strncmp_loop:
	ld	a, b
	or	c
	jr	z, _strncmp_eq	; n exhausted
	ld	a, (de)		; A = *str1
	sub	(hl)		; A = *str1 - *str2
	jr	nz, _strncmp_done
	; Equal. Check null.
	or	(hl)		; A = 0 | *str2 (which == *str1)
	jr	z, _strncmp_eq
	inc	de
	inc	hl
	dec	bc
	jr	_strncmp_loop
_strncmp_eq:
	xor	a
_strncmp_done:
	ld	c, a		; sign-extend A into BC
	ld	b, #0
	bit	7, a
	jr	z, _strncmp_ret
	ld	b, #0xFF
_strncmp_ret:
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip 2 bytes of stack args
	jp	(hl)
