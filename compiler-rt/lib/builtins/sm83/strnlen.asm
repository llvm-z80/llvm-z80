; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strnlen
	.globl _strnlen_loop

_strnlen:
	ld	h, d
	ld	l, e		; HL = str
	ld	d, b
	ld	e, c		; DE = maxlen (decreasing counter)
	ld	bc, #0		; length = 0
_strnlen_loop:
	ld	a, d
	or	e
	ret	z		; maxlen reached
	ld	a, (hl+)		; A = *str, HL++
	or	a
	ret	z		; null terminator found
	inc	bc
	dec	de
	jr	_strnlen_loop

;===------------------------------------------------------------------------===;
; _strcmp - Compare two null-terminated strings
;
; Input:  DE = str1, BC = str2
; Output: BC = negative/zero/positive (str1 <=> str2)
