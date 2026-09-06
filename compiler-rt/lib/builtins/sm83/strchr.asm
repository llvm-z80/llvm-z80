; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strchr

;===------------------------------------------------------------------------===;
; _strchr - Find character in string
;
; Input:  DE = string, BC = character (C = char)
; Output: BC = pointer to char, or 0 if not found
;===------------------------------------------------------------------------===;

_strchr:
	ld	h, d
	ld	l, e		; HL = string
_strchr_loop:
	ld	a, (hl)
	cp	c		; compare with target char
	jr	z, _strchr_found
	or	a		; check null terminator
	jr	z, _strchr_notfound
	inc	hl
	jr	_strchr_loop
_strchr_found:
	ld	c, l		; BC = HL (pointer to match)
	ld	b, h
	ret
_strchr_notfound:
	ld	bc, #0
	ret
