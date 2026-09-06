; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _roundf

;===------------------------------------------------------------------------===;
; _roundf - Round to nearest, ties away from zero
;
; Input:  DEBC = float
; Output: DEBC = round(x)
;
; round(x) = trunc(x + copysign(0.5, x))
;===------------------------------------------------------------------------===;
_roundf:
	; Build ±0.5f with sign of x: D[7] | 0x3F = 0x3F or 0xBF
	ld	a, d
	and	#0x80
	or	#0x3F
	ld	h, a		; H = D2 of ±0.5f
	ld	l, #0x00	; L = E2
	push	hl		; D2:E2
	ld	hl, #0x0000	; B2:C2
	push	hl
	; DEBC = x (arg1)
	call	___addsf3	; callee-cleanup, DEBC = x ± 0.5
	jp	_truncf
