; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___umodhi3

;===------------------------------------------------------------------------===;
; ___umodhi3 - 16-bit unsigned modulo
;
; Input:  DE = dividend, BC = divisor
; Output: BC = remainder
;===------------------------------------------------------------------------===;

___umodhi3:
	call	___udivhi3	; BC = quotient, HL = remainder
	ld	c, l		; BC = remainder
	ld	b, h
	ret
