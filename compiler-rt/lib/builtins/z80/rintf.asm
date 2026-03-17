; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _rintf
	.globl _nearbyintf
	.globl _roundevenf

;===------------------------------------------------------------------------===;
; _rintf / _nearbyintf / _roundevenf
; Round float to nearest integer, ties to even
;
; Input:  HLDE = float
; Output: HLDE = rounded float
;
; For Z80 (no FP exceptions), rintf == nearbyintf == roundevenf.
;
; Algorithm:
;   If |x| >= 2^23 or x == 0: return x (already integer)
;   If |x| < 0.5: return ±0.0
;   Add ±2^23 then subtract ±2^23. The add forces rounding to integer
;   (since f32 only has 23 mantissa bits), then subtract restores the value.
;   IEEE 754 round-to-nearest-even is the default rounding mode.
;===------------------------------------------------------------------------===;
_rintf:
_nearbyintf:
_roundevenf:
	; Extract exponent
	ld	a, h
	add	a, a
	ld	b, a
	ld	a, l
	rlca
	and	#1
	or	b		; A = exponent

	; exp == 0: ±0 or denormal → return x (±0.0 for denormal is ok)
	or	a
	ret	z

	; exp >= 150 (|x| >= 2^23): already integer
	cp	#150
	ret	nc

	; exp < 126 (|x| < 0.5): return ±0.0
	cp	#126
	jr	c, __rintf_zero

	; 126 <= exp < 150: add ±2^23 then subtract ±2^23
	; 2^23 = 0x4B000000
	; ±2^23: sign from x
	ld	a, h
	and	#0x80		; sign
	or	#0x4B		; 0x4B or 0xCB
	ld	b, a
	ld	c, #0x00

	; Push ±2^23 as arg2
	push	bc		; H2:L2
	ld	bc, #0x0000
	push	bc		; D2:E2
	; HLDE = x
	call	___addsf3	; callee-cleanup, HLDE = x + ±2^23 (rounded to integer)

	; Now subtract ±2^23 to get back
	; Need sign of result (same as original x)
	ld	a, h
	and	#0x80
	or	#0x4B
	ld	b, a
	ld	c, #0x00
	push	bc		; H2:L2 of ±2^23
	ld	bc, #0x0000
	push	bc		; D2:E2
	call	___subsf3	; callee-cleanup, HLDE = rounded integer
	ret

__rintf_zero:
	ld	a, h
	and	#0x80		; preserve sign
	ld	h, a
	ld	l, #0
	ld	d, l
	ld	e, l
	ret
