; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _bcmp

;===------------------------------------------------------------------------===;
; _bcmp - Compare memory blocks for equality only
;
; Input:  HL = ptr1, DE = ptr2, stack = size (i16)
; Output: DE = zero when equal, non-zero otherwise
;
; memcmp has to name which block is smaller, which costs it a re-read of the
; byte that differed and a branch on the carry. Equality alone needs neither.
;===------------------------------------------------------------------------===;
_bcmp:
	push	hl		; hold ptr1; HL is what reaches the stack
	ld	hl, #4		; past the saved ptr1 and the return address
	add	hl, sp
	ld	c, (hl)		; BC = size
	inc	hl
	ld	b, (hl)
	pop	hl		; HL = ptr1
	ld	a, b
	or	c
	jr	z, ___z80_bcmp_equal	; size == 0 compares equal
	ex	de, hl			; HL = ptr2 (CPI's pointer), DE = ptr1
___z80_bcmp_loop:
	ld	a, (de)			; A = *ptr1
	inc	de
	cpi				; A - *ptr2; HL++; BC--
	jr	nz, ___z80_bcmp_diff
	jp	pe, ___z80_bcmp_loop	; P/V stays set while BC != 0
___z80_bcmp_equal:
	ld	d, b			; BC reached zero on either path here
	ld	e, c
___z80_bcmp_ret:
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc		; re-push return address
	ret
___z80_bcmp_diff:
	ld	de, #1
	jr	___z80_bcmp_ret
