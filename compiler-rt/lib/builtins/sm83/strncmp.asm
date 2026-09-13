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
	jr	z, _strncmp_eq	; n exhausted, strings equal so far
	ld	a, (de)		; A = *str1
	cp	(hl)		; zero if equal, carry if *str1 < *str2
	jr	nz, _strncmp_diff
	or	a		; A is still *str1; zero means both ended
	jr	z, _strncmp_eq
	inc	de
	inc	hl
	dec	bc
	jr	_strncmp_loop
_strncmp_eq:
	ld	bc, #0
	jr	_strncmp_ret
_strncmp_diff:
	jr	c, _strncmp_less
	ld	bc, #1
	jr	_strncmp_ret
_strncmp_less:
	ld	bc, #0xFFFF
_strncmp_ret:
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip 2 bytes of stack args
	jp	(hl)
