; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strlen

;===------------------------------------------------------------------------===;
; _strlen - Get string length
;
; Input:  DE = pointer to null-terminated string
; Output: BC = length (number of bytes before null terminator)
; Uses LDI A,(HL) for auto-incrementing string scan.
;===------------------------------------------------------------------------===;

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
