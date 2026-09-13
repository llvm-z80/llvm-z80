; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memmove
	.globl ___z80_memmove_builtin

;===------------------------------------------------------------------------===;
; ___z80_memmove_builtin - Copy memory block (CallingConv::Z80_Builtin)
;
; Input:  DE = dest, BC = src, HL = size
; Output: none
;
;   dest < src : forward copy, ascending
;   dest > src : backward copy from the last byte of each region
;   dest == src or size == 0 : nothing to do
;
; SM83 has no 16-bit compare, so the direction test subtracts the low halves
; and borrows through the high ones; only the carry it leaves is used.
;===------------------------------------------------------------------------===;
___z80_memmove_builtin:
	ld	a, h
	or	l
	ret	z		; size == 0
	; Compare dest (DE) against src (BC).
	ld	a, e
	sub	c
	ld	a, d
	sbc	a, b		; carry set when dest < src
	jr	c, ___z80_memmove_fwd
	; dest >= src.  Equal pointers have nothing to move.
	ld	a, e
	sub	c
	jr	nz, ___z80_memmove_back
	ld	a, d
	sub	b
	ret	z		; dest == src
___z80_memmove_back:
	; Walk down from the last byte of each region.
	push	hl		; hold size
	ld	h, b
	ld	l, c		; HL = src
	pop	bc		; BC = size
	add	hl, bc
	dec	hl		; HL = src + size - 1
	push	hl		; hold src_end
	ld	h, d
	ld	l, e		; HL = dest
	add	hl, bc
	dec	hl		; HL = dest + size - 1
	pop	de		; DE = src_end
___z80_memmove_back_loop:
	ld	a, (de)
	dec	de
	ld	(hl-), a
	dec	bc
	ld	a, b
	or	c
	jr	nz, ___z80_memmove_back_loop
	ret
___z80_memmove_fwd:
	push	bc		; hold src
	ld	b, h
	ld	c, l		; BC = size
	pop	hl		; HL = src
___z80_memmove_fwd_loop:
	ld	a, (hl+)
	ld	(de), a
	inc	de
	dec	bc
	ld	a, b
	or	c
	jr	nz, ___z80_memmove_fwd_loop
	ret

;===------------------------------------------------------------------------===;
; _memmove - Copy memory block, C entry point
;
; Input:  DE = dest, BC = src, stack = size (i16)
; Output: BC = dest (original)
;===------------------------------------------------------------------------===;
_memmove:
	push	de		; save dest for return value
	ldhl	sp, #4		; [saved DE(2), ret addr(2), size]
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = size; DE and BC are already in place
	call	___z80_memmove_builtin
	pop	bc		; BC = original dest (return value)
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip 2 bytes of stack args
	jp	(hl)
