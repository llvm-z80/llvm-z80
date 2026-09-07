; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memset
	.globl ___z80_memset_builtin

;===------------------------------------------------------------------------===;
; ___z80_memset_builtin - Fill memory block (CallingConv::Z80_Builtin)
;
; Input:  DE = dest, BC = value (C = byte), HL = size
; Output: none
;
; The fill byte stays in A, so the count cannot pass through it: the size is
; split into two 8-bit counters, with B bumped when C is non-zero so the inner
; DEC C borrows into it.
;===------------------------------------------------------------------------===;
___z80_memset_builtin:
	ld	a, h
	or	l
	ret	z		; size == 0
	ld	a, c		; A = fill byte, held for the whole loop
	ld	b, h
	ld	c, l		; BC = size
	ld	h, d
	ld	l, e		; HL = dest, the pointer the loop steps
	inc	c
	dec	c
	jr	z, ___z80_memset_loop
	inc	b		; low count non-zero: one more outer pass
___z80_memset_loop:
	ld	(hl+), a
	dec	c
	jr	nz, ___z80_memset_loop
	dec	b
	jr	nz, ___z80_memset_loop
	ret

;===------------------------------------------------------------------------===;
; _memset - Fill memory block, C entry point
;
; Input:  DE = dest, BC = value (C = byte), stack = size (i16)
; Output: BC = dest (original)
;===------------------------------------------------------------------------===;
_memset:
	push	de		; save dest for return value
	ldhl	sp, #4		; [saved DE(2), ret addr(2), size]
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = size; DE and BC are already in place
	call	___z80_memset_builtin
	pop	bc		; BC = original dest (return value)
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip 2 bytes of stack args
	jp	(hl)
