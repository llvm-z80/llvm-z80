; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strcat
	.globl _strcat_find
	.globl _strcat_copy
	.globl _strcat_done

_strcat:
	push	de		; save dest for return
	; Find end of dest
	ld	h, d
	ld	l, e		; HL = dest
_strcat_find:
	ld	a, (hl+)		; A = *HL, HL++
	or	a
	jr	nz, _strcat_find
	dec	hl		; HL = null terminator of dest
	; Copy src. DE=end_of_dest, HL=src
	ld	d, h
	ld	e, l		; DE = end of dest (write target)
	ld	h, b
	ld	l, c		; HL = src (for ldi a,(hl))
_strcat_copy:
	ld	a, (hl+)		; A = *src, HL++
	ld	(de), a
	or	a
	jr	z, _strcat_done
	inc	de
	jr	_strcat_copy
_strcat_done:
	pop	bc		; BC = original dest
	ret

;===------------------------------------------------------------------------===;
; _strchr - Find character in string
;
; Input:  DE = string, BC = character (C = char)
; Output: BC = pointer to char, or 0 if not found
