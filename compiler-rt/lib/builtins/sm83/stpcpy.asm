; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _stpcpy
	.globl _stpcpy_loop
	.globl _stpcpy_done

_stpcpy:
	ld	h, b
	ld	l, c		; HL = src
_stpcpy_loop:
	ld	a, (hl+)		; A = *src, HL++
	ld	(de), a
	or	a
	jr	z, _stpcpy_done
	inc	de
	jr	_stpcpy_loop
_stpcpy_done:
	ld	c, e		; BC = DE (pointer to null terminator)
	ld	b, d
	ret

;===------------------------------------------------------------------------===;
; _strcat - Concatenate strings
;
; Input:  DE = dest, BC = src
; Output: BC = dest
; Uses LDI A,(HL) for scanning dest end and copying src.
