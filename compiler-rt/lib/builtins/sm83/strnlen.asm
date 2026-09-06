; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strnlen

;===------------------------------------------------------------------------===;
; _strnlen - Bounded string length
;
; Input:  DE = string, BC = maxlen
; Output: BC = min(strlen(s), maxlen)
;===------------------------------------------------------------------------===;

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
