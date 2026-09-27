; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___umodhi3_fast

;===------------------------------------------------------------------------===;
; ___umodhi3_fast - 16-bit unsigned modulo, speed variant (ravn/llvm-z80 #244)
; Identical to ___umodhi3 but calls the fully-unrolled ___udivhi3_fast.
; Input:  HL = dividend, DE = divisor
; Output: DE = remainder
;===------------------------------------------------------------------------===;
___umodhi3_fast:
	call	___udivhi3_fast	; DE = quotient, HL = remainder
	ex	de, hl		; DE = remainder
	ret
