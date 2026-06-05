; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___umodqi3

;===------------------------------------------------------------------------===;
; ___umodqi3 - 8-bit unsigned modulo (remainder)
;
; Input:  A = dividend, E = divisor
; Output: A = remainder
; Clobbers: B, D, FLAGS
;===------------------------------------------------------------------------===;
___umodqi3:
	ld	d, a		; D = dividend
	xor	a		; A = 0 (remainder)
	ld	b, #8		; 8-bit counter
___umodqi3_loop:
	sla	d		; shift dividend MSB -> carry
	rla			; remainder = remainder*2 + carry
	cp	e		; compare remainder with divisor
	jr	c, ___umodqi3_skip
	sub	e		; remainder -= divisor
	inc	d		; set quotient bit
___umodqi3_skip:
	djnz	___umodqi3_loop
	; A = remainder (already in A)
	ret
