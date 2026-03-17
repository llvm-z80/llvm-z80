; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___umodhi3

;===------------------------------------------------------------------------===;
; ___umodhi3 - 16-bit unsigned modulo
;
; Input:  HL = dividend, DE = divisor
; Output: DE = remainder
;===------------------------------------------------------------------------===;
___umodhi3:
	call	___udivhi3	; DE = quotient, HL = remainder
	ex	de, hl		; DE = remainder
	ret

;===------------------------------------------------------------------------===;
; ___modhi3 - 16-bit signed modulo
;
; Input:  HL = dividend, DE = divisor
