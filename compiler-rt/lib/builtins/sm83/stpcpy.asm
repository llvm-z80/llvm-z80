; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _stpcpy

;===------------------------------------------------------------------------===;
; _stpcpy - Copy string, return pointer to end of dest
;
; Input:  DE = dest, BC = src
; Output: BC = pointer to null terminator in dest
;===------------------------------------------------------------------------===;

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
