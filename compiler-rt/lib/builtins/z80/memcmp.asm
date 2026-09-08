; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memcmp

;===------------------------------------------------------------------------===;
; ___z80_memcmp_builtin - Compare memory blocks, shared body
;
; Input:  HL = ptr1, DE = ptr2, BC = size
; Output: DE = negative / zero / positive
;
; CPI compares A against (HL), steps HL and drops BC in one instruction, and
; leaves P/V set while BC is non-zero.
;
; The result carries the sign of an UNSIGNED byte comparison, so it comes from
; the carry flag: sign-extending the difference would make 0x00 against 0xFF
; positive.
;===------------------------------------------------------------------------===;
___z80_memcmp_builtin:
	ld	a, b
	or	c
	jr	z, ___z80_memcmp_eq	; size == 0 compares equal
	ex	de, hl			; HL = ptr2 (CPI's pointer), DE = ptr1
___z80_memcmp_loop:
	ld	a, (de)			; A = *ptr1
	inc	de
	cpi				; A - *ptr2; HL++; BC--
	jr	nz, ___z80_memcmp_diff
	jp	pe, ___z80_memcmp_loop	; P/V stays set while BC != 0
___z80_memcmp_eq:
	ld	de, #0
	ret
___z80_memcmp_diff:
	dec	hl			; back to the byte that differed
	cp	(hl)			; carry set when *ptr1 < *ptr2
	jr	c, ___z80_memcmp_less
	ld	de, #1
	ret
___z80_memcmp_less:
	ld	de, #0xFFFF
	ret

;===------------------------------------------------------------------------===;
; _memcmp - Compare memory blocks, C entry point
;
; Input:  HL = ptr1, DE = ptr2, stack = size (i16)
; Output: DE = negative / zero / positive
;===------------------------------------------------------------------------===;
_memcmp:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, 4(ix)	; BC = size
	ld	b, 5(ix)
	call	___z80_memcmp_builtin
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc		; re-push return address
	ret
