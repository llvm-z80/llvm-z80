; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strchr

;===------------------------------------------------------------------------===;
; _strchr - Find character in string
;
; Input:  HL = string, DE = character (E = char)
; Output: DE = pointer to char, or 0 if not found
;===------------------------------------------------------------------------===;

_strchr:
_strchr_loop:
	ld	a, (hl)
	cp	e
	jr	z, _strchr_found
	or	a
	jr	z, _strchr_notfound
	inc	hl
	jr	_strchr_loop
_strchr_found:
	ex	de, hl		; DE = pointer to match
	ret
_strchr_notfound:
	ld	de, #0
	ret
