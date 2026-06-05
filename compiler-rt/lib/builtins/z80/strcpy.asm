; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strcpy
	.globl _strcpy_loop
	.globl _strcpy_done

;===------------------------------------------------------------------------===;
; _strcpy - Copy string
;
; Input:  HL = dest, DE = src
; Output: DE = dest
;===------------------------------------------------------------------------===;
_strcpy:
	push	hl		; save dest for return
	ex	de, hl		; HL = src, DE = dest
_strcpy_loop:
	ld	a, (hl)
	ld	(de), a
	or	a
	jr	z, _strcpy_done
	inc	hl
	inc	de
	jr	_strcpy_loop
_strcpy_done:
	pop	de		; DE = original dest
	ret

;===------------------------------------------------------------------------===;
; _strncpy - Copy string with bound (pads with nulls)
;
; Input:  HL = dest, DE = src, stack = n (i16)
