; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memcmp

;===------------------------------------------------------------------------===;
; ___z80_memcmp_builtin - Compare memory blocks, shared body
;
; Input:  DE = ptr1, BC = ptr2, HL = size
; Output: BC = negative / zero / positive
;
; SM83 can only compare A against (HL), so ptr2 moves there and ptr1 is read
; through DE.
;
; The result carries the sign of an UNSIGNED byte comparison, so it comes from
; the carry flag: sign-extending the difference would make 0x00 against 0xFF
; positive.
;===------------------------------------------------------------------------===;
___z80_memcmp_builtin:
	ld	a, h
	or	l
	jr	z, ___z80_memcmp_eq	; size == 0 compares equal
	push	bc		; hold ptr2
	ld	b, h
	ld	c, l		; BC = size
	pop	hl		; HL = ptr2
___z80_memcmp_loop:
	ld	a, (de)		; A = *ptr1
	cp	(hl)		; zero if equal, carry if *ptr1 < *ptr2
	jr	nz, ___z80_memcmp_diff
	inc	de
	inc	hl
	dec	bc
	ld	a, b
	or	c
	jr	nz, ___z80_memcmp_loop
___z80_memcmp_eq:
	ld	bc, #0
	ret
___z80_memcmp_diff:
	jr	c, ___z80_memcmp_less
	ld	bc, #1
	ret
___z80_memcmp_less:
	ld	bc, #0xFFFF
	ret

;===------------------------------------------------------------------------===;
; _memcmp - Compare memory blocks, C entry point
;
; Input:  DE = ptr1, BC = ptr2, stack = size (i16)
; Output: BC = negative / zero / positive
;===------------------------------------------------------------------------===;
_memcmp:
	ldhl	sp, #2		; [ret addr(2), size]
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = size; DE and BC are already in place
	call	___z80_memcmp_builtin
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip 2 bytes of stack args
	jp	(hl)
