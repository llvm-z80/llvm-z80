; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memcpy
	.globl _memcpy_loop
	.globl _memcpy_done

_memcpy:
	push	de		; save dest for return
	; Load size from stack: [saved_DE(2), ret_addr(2), size_lo, size_hi]
	ldhl	sp, #4
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = size
	; Rearrange: HL=src, DE=dest, BC=size
	push	bc		; save src
	ld	b, h
	ld	c, l		; BC = size
	pop	hl		; HL = src
	ld	a, b
	or	c
	jr	z, _memcpy_done
_memcpy_loop:
	ld	a, (hl+)	; A = *src, HL++
	ld	(de), a		; *dest = A
	inc	de
	dec	bc
	ld	a, b
	or	c
	jr	nz, _memcpy_loop
_memcpy_done:
	pop	bc		; BC = original dest (return value)
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip 2 bytes of stack args
	jp	(hl)

;===------------------------------------------------------------------------===;
; _memmove - Copy memory block (handles overlapping regions)
;
; Input:  DE = dest, BC = src, stack = size (i16)
; Output: BC = dest (original)
; If dest < src: forward copy using LDI A,(HL)
; If dest >= src: backward copy using LDD A,(HL)
