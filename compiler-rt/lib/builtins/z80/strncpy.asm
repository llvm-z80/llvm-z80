; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strncpy
	.globl _strncpy_loop
	.globl _strncpy_pad
	.globl _strncpy_done

;===------------------------------------------------------------------------===;
; _strncpy - Copy string with bound (pads with nulls)
;
; Input:  HL = dest, DE = src, stack = n (i16)
; Output: DE = dest
;===------------------------------------------------------------------------===;
_strncpy:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, 4(ix)	; BC = n
	ld	b, 5(ix)
	push	hl		; save dest
	ex	de, hl		; HL = src, DE = dest
_strncpy_loop:
	ld	a, b
	or	c
	jr	z, _strncpy_done
	ld	a, (hl)
	ld	(de), a
	or	a
	jr	z, _strncpy_pad
	inc	hl
	inc	de
	dec	bc
	jr	_strncpy_loop
_strncpy_pad:
	inc	de
	dec	bc
	ld	a, b
	or	c
	jr	z, _strncpy_done
	xor	a
	ld	(de), a
	jr	_strncpy_pad
_strncpy_done:
	pop	de		; DE = original dest
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc
	ret

;===------------------------------------------------------------------------===;
; _stpcpy - Copy string, return pointer to end of dest
;
; Input:  HL = dest, DE = src
; Output: DE = pointer to null terminator in dest
