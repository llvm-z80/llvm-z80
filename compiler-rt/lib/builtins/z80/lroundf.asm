; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _lroundf

;===------------------------------------------------------------------------===;
; _lroundf - Round float to nearest integer (ties away from zero), return i32
;
; Input:  HLDE = float
; Output: HLDE = (long)round(x)
;
; Algorithm:
;   1. Add ±0.5 (same sign as x)
;   2. Truncate to integer using fixsfsi
;===------------------------------------------------------------------------===;
_lroundf:
	; Add ±0.5f with same sign as x
	ld	a, h
	and	#0x80
	or	#0x3F		; A = 0x3F (+0.5) or 0xBF (-0.5)
	ld	b, a
	ld	c, #0x00
	push	bc		; H2:L2 of ±0.5f
	ld	bc, #0x0000
	push	bc		; D2:E2 of ±0.5f
	; HLDE = x (arg1)
	call	___addsf3	; callee-cleanup, HLDE = x ± 0.5
	; Convert to i32
	jp	___fixsfsi
