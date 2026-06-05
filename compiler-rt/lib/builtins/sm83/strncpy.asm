; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strncpy
	.globl _strncpy_loop
	.globl _strncpy_pad
	.globl _strncpy_done

_strncpy:
	push	de		; save dest for return
	; Load n: [saved_DE(2), ret_addr(2), n_lo, n_hi]
	ldhl	sp, #4
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = n
	; Rearrange: HL=src, DE=dest, BC=n
	push	bc		; save src
	ld	b, h
	ld	c, l		; BC = n
	pop	hl		; HL = src
_strncpy_loop:
	ld	a, b
	or	c
	jr	z, _strncpy_done
	ld	a, (hl+)		; A = *src, HL++
	ld	(de), a
	or	a
	jr	z, _strncpy_pad	; hit null, pad rest
	inc	de
	dec	bc
	jr	_strncpy_loop
_strncpy_pad:
	; Pad remaining bytes with nulls
	inc	de
	dec	bc
	ld	a, b
	or	c
	jr	z, _strncpy_done
	xor	a
	ld	(de), a
	jr	_strncpy_pad
_strncpy_done:
	pop	bc		; BC = original dest (return value)
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip 2 bytes of stack args
	jp	(hl)

;===------------------------------------------------------------------------===;
; _stpcpy - Copy string, return pointer to end of dest
;
; Input:  DE = dest, BC = src
; Output: BC = pointer to null terminator in dest
