; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___modhi3_fast

;===------------------------------------------------------------------------===;
; ___modhi3_fast - 16-bit signed modulo, speed variant (ravn/llvm-z80 #244)
; Identical to ___modhi3 but calls the fully-unrolled ___udivhi3_fast.
; Input:  HL = dividend, DE = divisor
; Output: DE = remainder (same sign as dividend, C99)
;===------------------------------------------------------------------------===;
___modhi3_fast:
	ld	a, h
	push	af
	bit	7, h
	jr	z, .Lpos_num
	xor	a
	sub	l
	ld	l, a
	sbc	a, a
	sub	h
	ld	h, a
.Lpos_num:
	bit	7, d
	jr	z, .Lpos_den
	xor	a
	sub	e
	ld	e, a
	sbc	a, a
	sub	d
	ld	d, a
.Lpos_den:
	call	___udivhi3_fast	; DE = quotient, HL = remainder
	ex	de, hl		; DE = remainder
	pop	af
	bit	7, a
	ret	z
	xor	a
	sub	e
	ld	e, a
	sbc	a, a
	sub	d
	ld	d, a
	ret
