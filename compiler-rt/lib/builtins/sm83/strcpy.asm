; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strcpy

;===------------------------------------------------------------------------===;
; _strcpy - Copy string
;
; Input:  DE = dest, BC = src
; Output: BC = dest
; Uses LDI A,(HL) for auto-incrementing source reads.
;===------------------------------------------------------------------------===;

_strcpy:
	push	de		; save dest for return
	ld	h, b
	ld	l, c		; HL = src
_strcpy_loop:
	ld	a, (hl+)		; A = *src, HL++
	ld	(de), a
	or	a
	jr	z, _strcpy_done
	inc	de
	jr	_strcpy_loop
_strcpy_done:
	pop	bc		; BC = original dest
	ret
