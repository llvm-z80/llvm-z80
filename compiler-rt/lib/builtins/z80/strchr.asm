; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strchr
	.globl _strchr_loop
	.globl _strchr_found
	.globl _strchr_notfound

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

;===------------------------------------------------------------------------===;
; _memcmp - Compare memory blocks
;
; Input:  HL = ptr1, DE = ptr2, stack = size (i16)
; Output: DE = negative/zero/positive
