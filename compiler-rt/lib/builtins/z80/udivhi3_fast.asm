; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___udivhi3_fast
	.globl ___udivhi3

;===------------------------------------------------------------------------===;
; ___udivhi3_fast - 16-bit unsigned division, speed variant (ravn/llvm-z80 #244)
;
; Identical result to ___udivhi3, selected by the backend only at -O3 on Z80.
;
; Small quotients dominate division-heavy code (e.g. the digits-of-e kernel,
; where each x/n has x < ~10*n so the quotient is < ~10).  For those we use
; REPEATED SUBTRACTION -- a handful of full 16-bit `sbc hl,de` steps -- which
; beats a fixed bit-loop and is correct by construction.  A small cap bounds
; the wasted work; on cap we delegate to the bounded, correct small routine
; (___udivhi3) so a large quotient is never slower than -Os by more than the
; cap's worth of subtractions.
;
; Input:  HL = dividend, DE = divisor
; Output: DE = quotient, HL = remainder
;===------------------------------------------------------------------------===;
___udivhi3_fast:
	push	hl		; save dividend for the cap fallback
	ld	bc, #0x1000	; B = 16 (attempt cap), C = 0 (quotient counter)
.Lrs:
	or	a		; CF = 0 for a clean subtract
	sbc	hl, de		; HL -= divisor
	jr	c, .Lrsdone	; borrow: overshot, quotient found
	inc	c		; another whole divisor subtracted
	djnz	.Lrs
	;; cap reached: quotient may be large, delegate to the small routine
	pop	hl		; restore original dividend (DE still = divisor)
	jp	___udivhi3
.Lrsdone:
	add	hl, de		; undo the overshoot -> HL = remainder
	ld	d, #0
	ld	e, c		; DE = quotient (<= cap)
	pop	bc		; discard saved dividend
	ret
