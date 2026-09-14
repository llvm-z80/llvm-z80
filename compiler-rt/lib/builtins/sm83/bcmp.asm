; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _bcmp

;===------------------------------------------------------------------------===;
; _bcmp - Compare memory blocks for equality only
;
; Input:  DE = ptr1, BC = ptr2, stack = size (i16)
; Output: BC = zero when equal, non-zero otherwise
;
; memcmp has to name which block is smaller, which costs it a branch on the
; carry and a second answer to build. Equality alone needs neither.
;===------------------------------------------------------------------------===;
_bcmp:
	ldhl	sp, #2		; [ret addr(2), size]
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = size; DE and BC are already in place
	ld	a, h
	or	l
	jr	z, ___z80_bcmp_zero	; size == 0 compares equal
	push	bc		; hold ptr2
	ld	b, h
	ld	c, l		; BC = size
	pop	hl		; HL = ptr2
___z80_bcmp_loop:
	ld	a, (de)		; A = *ptr1
	cp	(hl)
	jr	nz, ___z80_bcmp_diff
	inc	de
	inc	hl
	dec	bc
	ld	a, b
	or	c
	jr	nz, ___z80_bcmp_loop
___z80_bcmp_ret:		; BC counted down to zero, which is the answer
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip 2 bytes of stack args
	jp	(hl)
___z80_bcmp_zero:
	ld	b, h		; size was zero, so HL is too
	ld	c, l
	jr	___z80_bcmp_ret
___z80_bcmp_diff:
	ld	bc, #1
	jr	___z80_bcmp_ret
