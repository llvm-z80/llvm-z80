; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___udivqi3

;===------------------------------------------------------------------------===;
; ___udivqi3 - 8-bit unsigned division (quotient)
;
; Input:  A = dividend, E = divisor
; Output: A = quotient
; Clobbers: B, D, FLAGS
;===------------------------------------------------------------------------===;
___udivqi3:
	ld	d, a		; D = dividend
	xor	a		; A = 0 (remainder)
	ld	b, #8		; 8-bit counter
___udivqi3_loop:
	sla	d		; shift dividend MSB -> carry
	rla			; remainder = remainder*2 + carry
	cp	e		; compare remainder with divisor
	jr	c, ___udivqi3_skip
	sub	e		; remainder -= divisor
	inc	d		; set quotient bit
___udivqi3_skip:
	djnz	___udivqi3_loop
	ld	a, d		; A = quotient
	ret
