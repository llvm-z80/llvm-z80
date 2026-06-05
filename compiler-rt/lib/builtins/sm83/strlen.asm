; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strlen
	.globl _strlen_loop

_strlen:
	ld	h, d
	ld	l, e		; HL = str
	ld	bc, #0		; length = 0
_strlen_loop:
	ld	a, (hl+)		; A = *str, HL++
	or	a
	ret	z		; found null, BC = length
	inc	bc
	jr	_strlen_loop

;===------------------------------------------------------------------------===;
; _strnlen - Bounded string length
;
; Input:  DE = string, BC = maxlen
; Output: BC = min(strlen(s), maxlen)
