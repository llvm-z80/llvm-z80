; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memmove
	.globl _memmove_bwd_loop
	.globl _memmove_fwd
	.globl _memmove_fwd_loop
	.globl _memmove_done

_memmove:
	push	de		; save dest for return
	; Load size
	ldhl	sp, #4
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = size
	ld	a, h
	or	l
	jr	z, _memmove_done	; size == 0
	; Direction check: dest(DE) vs src(BC), unsigned
	ld	a, e
	sub	c
	ld	a, d
	sbc	a, b		; carry if dest < src
	jr	c, _memmove_fwd
	; === Backward copy: dest >= src ===
	; HL=size, DE=dest, BC=src
	; Compute end pointers: dest_end = DE+HL-1, src_end = BC+HL-1
	dec	hl		; HL = size - 1
	push	hl		; [1] save size-1
	add	hl, de		; HL = dest + size - 1 = dest_end
	ld	d, h
	ld	e, l		; DE = dest_end
	pop	hl		; [1] HL = size-1
	push	de		; [2] save dest_end
	push	hl		; [3] save size-1 (for counter)
	add	hl, bc		; HL = src + size - 1 = src_end
	pop	bc		; [3] BC = size-1
	inc	bc		; BC = size (counter)
	pop	de		; [2] DE = dest_end
	; HL=src_end, DE=dest_end, BC=size
_memmove_bwd_loop:
	ld	a, (hl-)		; A = *src_end, HL--
	ld	(de), a
	dec	de
	dec	bc
	ld	a, b
	or	c
	jr	nz, _memmove_bwd_loop
	jr	_memmove_done
_memmove_fwd:
	; === Forward copy: dest < src ===
	; HL=size, DE=dest, BC=src
	push	bc		; save src
	ld	b, h
	ld	c, l		; BC = size
	pop	hl		; HL = src
_memmove_fwd_loop:
	ld	a, (hl+)		; A = *src, HL++
	ld	(de), a
	inc	de
	dec	bc
	ld	a, b
	or	c
	jr	nz, _memmove_fwd_loop
_memmove_done:
	pop	bc		; BC = original dest (return value)
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip 2 bytes of stack args
	jp	(hl)

;===------------------------------------------------------------------------===;
; _memset - Fill memory block
;
; Input:  DE = dest, BC = value (C = byte), stack = size (i16)
; Output: BC = dest (original)
; Uses E as fill byte holder, freeing A for the loop counter check.
