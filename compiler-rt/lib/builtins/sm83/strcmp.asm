; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strcmp

;===------------------------------------------------------------------------===;
; _strcmp - Compare two null-terminated strings
;
; Input:  DE = str1, BC = str2
; Output: BC = negative/zero/positive (str1 <=> str2)
;===------------------------------------------------------------------------===;

_strcmp:
	ld	h, d
	ld	l, e		; HL = str1
	ld	d, b
	ld	e, c		; DE = str2
_strcmp_loop:
	ld	a, (de)		; A = *str2
	ld	c, a		; C = *str2
	ld	a, (hl+)	; A = *str1, HL++
	cp	c		; zero if equal, carry if *str1 < *str2
	jr	nz, _strcmp_diff
	or	a		; A is still *str1; zero means both ended
	jr	z, _strcmp_eq
	inc	de
	jr	_strcmp_loop
_strcmp_eq:
	ld	bc, #0
	ret
_strcmp_diff:
	jr	c, _strcmp_less
	ld	bc, #1
	ret
_strcmp_less:
	ld	bc, #0xFFFF
	ret
