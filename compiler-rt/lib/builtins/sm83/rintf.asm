; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _rintf
	.globl _nearbyintf
	.globl _roundevenf

;===------------------------------------------------------------------------===;
; _rintf / _nearbyintf / _roundevenf
; Round to nearest integer, ties to even (SM83)
;
; Input:  DEBC = float
; Output: DEBC = rounded float
;
; Add ±2^23 then subtract ±2^23 to force IEEE round-to-nearest-even.
;===------------------------------------------------------------------------===;
_rintf:
_nearbyintf:
_roundevenf:
	ld	a, d
	add	a, a
	ld	h, a
	ld	a, e
	rlca
	and	#1
	or	h		; A = exponent

	or	a
	ret	z		; ±0 or denormal

	cp	#150
	ret	nc		; already integer

	cp	#126
	jr	c, __rintf_zero	; |x| < 0.5

	; Add ±2^23 (0x4B000000)
	ld	a, d
	and	#0x80
	or	#0x4B
	ld	h, a
	ld	l, #0x00
	push	hl		; D2:E2
	ld	hl, #0x0000
	push	hl		; B2:C2
	call	___addsf3	; callee-cleanup

	; Subtract ±2^23
	ld	a, d
	and	#0x80
	or	#0x4B
	ld	h, a
	ld	l, #0x00
	push	hl
	ld	hl, #0x0000
	push	hl
	call	___subsf3	; callee-cleanup
	ret

__rintf_zero:
	ld	a, d
	and	#0x80
	ld	d, a
	ld	e, #0
	ld	b, e
	ld	c, e
	ret
