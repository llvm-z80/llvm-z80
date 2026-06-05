; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memchr
	.globl _memchr_loop
	.globl _memchr_found
	.globl _memchr_notfound

_memchr:
	; Load size: [ret_addr(2), size_lo, size_hi]
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = size
	; Rearrange: HL=ptr, E=search byte, BC=size
	push	hl		; save size
	ld	h, d
	ld	l, e		; HL = ptr
	ld	e, c		; E = search byte
	pop	bc		; BC = size
_memchr_loop:
	ld	a, b
	or	c
	jr	z, _memchr_notfound
	ld	a, (hl)
	cp	e		; compare with search byte
	jr	z, _memchr_found
	inc	hl
	dec	bc
	jr	_memchr_loop
_memchr_found:
	ld	c, l		; BC = HL (pointer to match)
	ld	b, h
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip 2 bytes of stack args
	jp	(hl)
_memchr_notfound:
	ld	bc, #0
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip 2 bytes of stack args
	jp	(hl)

;===------------------------------------------------------------------------===;
; _bzero - Zero out memory block
;
; Input:  DE = ptr, BC = size
; Output: BC = ptr
; Uses E as zero holder for efficient loop.
