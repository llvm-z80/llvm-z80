; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strcmp
	.globl _strcmp_loop
	.globl _strcmp_done

_strcmp:
	ld	h, d
	ld	l, e		; HL = str1
	ld	d, b
	ld	e, c		; DE = str2
_strcmp_loop:
	ld	a, (de)		; A = *str2
	ld	c, a		; C = *str2
	ld	a, (hl+)		; A = *str1, HL++
	sub	c		; A = *str1 - *str2
	jr	nz, _strcmp_done
	; Equal so far. Check if both null.
	or	c		; A = 0 | *str2 (== *str1 since equal)
	jr	z, _strcmp_done	; both null, A = 0
	inc	de
	jr	_strcmp_loop
_strcmp_done:
	; Sign-extend A into BC
	ld	c, a
	ld	b, #0
	bit	7, a
	ret	z
	ld	b, #0xFF
	ret

;===------------------------------------------------------------------------===;
; _strncmp - Compare two strings up to n bytes
;
; Input:  DE = str1, BC = str2, stack = n (i16)
; Output: BC = negative/zero/positive
; Uses SUB (HL) for comparing bytes directly from memory.
