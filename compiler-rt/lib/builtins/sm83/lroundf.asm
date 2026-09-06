; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _lroundf

;===------------------------------------------------------------------------===;
; _lroundf - Round float to nearest integer (ties away from zero), return i32
;
; Input:  DEBC = float
; Output: DEBC = (long)round(x)
;===------------------------------------------------------------------------===;
_lroundf:
	; Add ±0.5f with sign of x
	ld	a, d
	and	#0x80
	or	#0x3F
	ld	h, a		; H = 0x3F or 0xBF
	ld	l, #0x00
	push	hl		; D2:E2
	ld	hl, #0x0000
	push	hl		; B2:C2
	call	___addsf3	; callee-cleanup, DEBC = x ± 0.5
	jp	___fixsfsi
