; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _stpcpy
	.globl _stpcpy_loop

_stpcpy:
	ex	de, hl		; HL = src, DE = dest
_stpcpy_loop:
	ld	a, (hl)
	ld	(de), a
	or	a
	ret	z		; DE points to null terminator
	inc	hl
	inc	de
	jr	_stpcpy_loop

;===------------------------------------------------------------------------===;
; _strcat - Concatenate strings
;
; Input:  HL = dest, DE = src
; Output: DE = dest
