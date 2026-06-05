; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strnlen
	.globl _strnlen_loop

;===------------------------------------------------------------------------===;
; _strnlen - Bounded string length
;
; Input:  HL = string, DE = maxlen
; Output: DE = min(strlen(s), maxlen)
;===------------------------------------------------------------------------===;
_strnlen:
	ld	b, d		; BC = maxlen
	ld	c, e
	ld	de, #0		; length = 0
_strnlen_loop:
	ld	a, b
	or	c
	ret	z		; maxlen reached
	ld	a, (hl)
	or	a
	ret	z		; null terminator found
	inc	hl
	inc	de
	dec	bc
	jr	_strnlen_loop
