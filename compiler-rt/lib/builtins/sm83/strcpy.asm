; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strcpy
	.globl _strcpy_loop
	.globl _strcpy_done

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

;===------------------------------------------------------------------------===;
; _strncpy - Copy string with bound (pads with nulls)
;
; Input:  DE = dest, BC = src, stack = n (i16)
; Output: BC = dest
