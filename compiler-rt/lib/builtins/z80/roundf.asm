; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _roundf

;===------------------------------------------------------------------------===;
; _roundf - Round to nearest, ties away from zero
;
; Input:  HLDE = float
; Output: HLDE = round(x)
;
; Algorithm:
;   round(x) = floor(x + 0.5)  if x >= 0
;   round(x) = ceil(x - 0.5)   if x < 0
;
; Simplified: round(x) = trunc(x + copysign(0.5, x))
;   Add 0.5 with the same sign as x, then truncate.
;
; __addsf3 convention: arg1=HLDE, arg2=stack (callee-cleanup), returns HLDE.
;===------------------------------------------------------------------------===;
_roundf:
	; Extract sign for ±0.5
	ld	a, h
	and	#0x80		; A = sign bit (0x00 or 0x80)
	or	#0x3F		; A = 0x3F (positive) or 0xBF (negative)
	ld	b, a		; B = H byte of ±0.5f

	; Push ±0.5f as arg2 on stack (H2:L2 first, then D2:E2)
	ld	c, #0x00	; C = L byte of 0.5f = 0x00
	push	bc		; H2:L2
	ld	bc, #0x0000	; D2:E2
	push	bc

	; HLDE still = x (arg1). Call __addsf3(x, ±0.5)
	call	___addsf3	; callee-cleanup, result in HLDE

	; Truncate the result
	jp	_truncf
