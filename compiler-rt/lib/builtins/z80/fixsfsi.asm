; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___fixsfsi
	.globl __fsi_not_nan
	.globl __fsi_lsh_lp
	.globl __fsi_rshift
	.globl __fsi_rsh_lp
	.globl __fsi_sign
	.globl __fsi_zero
	.globl __fsi_zero_nr
	.globl __fsi_overflow
	.globl __fsi_ovf_neg
	.globl ___fixunssfsi
	.globl __fusi_not_nan
	.globl __fusi_lsh_lp
	.globl __fusi_rsh
	.globl __fusi_rsh_lp
	.globl __fusi_done
	.globl __fusi_zero
	.globl __fusi_overflow

;      shift = exp - 127 - 23 = exp - 150
;      If shift >= 0: left shift mantissa by shift
;      If shift < 0: right shift mantissa by -shift
;   5. Apply sign (negate if negative)
;===------------------------------------------------------------------------===;
___fixsfsi:
	; --- NaN check: exp=255 and mantissa!=0 → return 0 ---
	ld	a, h
	and	#0x7F
	cp	#0x7F
	jr	nz, __fsi_not_nan
	bit	7, l
	jr	z, __fsi_not_nan
	; exp bits = 0xFF, check mantissa
	ld	a, l
	and	#0x7F
	or	d
	or	e
	jr	nz, __fsi_zero_nr	; NaN → return 0 (no pop needed)
__fsi_not_nan:

	; Extract sign
	ld	a, h
	and	#0x80
	push	af		; save sign on stack

	; Extract exponent
	ld	a, h
	add	a, a		; A = exp[7:1] << 1
	ld	b, a
	ld	a, l
	rlca
	and	#1
	or	b		; A = full exponent
	ld	b, a		; B = exponent (save before modifying L)
	or	a
	jr	z, __fsi_zero	; exp=0 → value is ±0 or denormal (< 1.0)

	; Check if |value| < 1.0 (exp < 127)
	cp	#127
	jr	c, __fsi_zero

	; Check overflow: exp >= 127+31 = 158
	cp	#158
	jr	nc, __fsi_overflow

	; Extract mantissa with implicit bit into L:D:E
	ld	a, l
	or	#0x80		; set implicit bit (L[7] = 1)
	ld	l, a
	ld	h, #0		; H:L:D:E = 32-bit mantissa

	; Calculate shift: exp - 150
	ld	a, b		; A = exponent (from B)
	sub	#150		; A = exp - 150 (signed)
	jr	z, __fsi_sign	; no shift needed
	jr	c, __fsi_rshift	; negative → right shift

	; Left shift: A = shift amount (1..7)
	ld	b, a
__fsi_lsh_lp:
	sla	e
	rl	d
	rl	l
	rl	h
	djnz	__fsi_lsh_lp
	jr	__fsi_sign

__fsi_rshift:
	; Right shift: A = -(shift amount), negate to get count
	neg			; A = shift amount (1..23)
	ld	b, a
__fsi_rsh_lp:
	srl	h
	rr	l
	rr	d
	rr	e
	djnz	__fsi_rsh_lp

__fsi_sign:
	; Apply sign
	pop	af		; restore sign
	and	#0x80
	ret	z		; positive, done

	; Negate HLDE (two's complement)
	ld	a, e
	cpl
	ld	e, a
	ld	a, d
	cpl
	ld	d, a
	ld	a, l
	cpl
	ld	l, a
	ld	a, h
	cpl
	ld	h, a
	inc	e
	ret	nz
	inc	d
	ret	nz
	inc	l
	ret	nz
	inc	h
	ret

__fsi_zero:
	pop	af		; discard saved sign
__fsi_zero_nr:
	ld	hl, #0
	ld	d, h
	ld	e, l
	ret

__fsi_overflow:
	; Clamp to INT32_MAX (0x7FFFFFFF) or INT32_MIN (0x80000000)
	pop	af
	and	#0x80
	jr	nz, __fsi_ovf_neg
	; Positive overflow → INT32_MAX
	ld	hl, #0x7FFF
	ld	de, #0xFFFF
	ret
__fsi_ovf_neg:
	; Negative overflow → INT32_MIN
	ld	hl, #0x8000
	ld	de, #0x0000
	ret

;===------------------------------------------------------------------------===;
; ___fixunssfsi - Convert float to unsigned int32
;
; Input:  HLDE = float
; Output: HLDE = unsigned int32
;
; Same as ___fixsfsi but:
;   - Negative input → return 0
;   - No sign handling
;   - Overflow at exp >= 127+32 = 159
;===------------------------------------------------------------------------===;
___fixunssfsi:
	; --- NaN check: exp=255 and mantissa!=0 → return 0 ---
	ld	a, h
	and	#0x7F
	cp	#0x7F
	jr	nz, __fusi_not_nan
	bit	7, l
	jr	z, __fusi_not_nan
	ld	a, l
	and	#0x7F
	or	d
	or	e
	jr	nz, __fusi_zero	; NaN → return 0
__fusi_not_nan:

	; Check sign: negative → return 0
	bit	7, h
	jr	nz, __fusi_zero

	; Extract exponent
	ld	a, h
	add	a, a
	ld	b, a
	ld	a, l
	rlca
	and	#1
	or	b		; A = exponent
	ld	b, a		; B = exponent (save before modifying L)
	or	a
	jr	z, __fusi_zero	; exp=0 → ±0 or denormal

	cp	#127
	jr	c, __fusi_zero	; |value| < 1.0

	; Check overflow: exp >= 159 (value >= 2^32)
	cp	#159
	jr	nc, __fusi_overflow

	; Extract mantissa with implicit bit
	ld	a, l
	or	#0x80
	ld	l, a
	ld	h, #0		; H:L:D:E = 32-bit mantissa

	ld	a, b		; A = exponent
	sub	#150		; A = exp - 150
	jr	z, __fusi_done
	jr	c, __fusi_rsh

	; Left shift
	ld	b, a
__fusi_lsh_lp:
	sla	e
	rl	d
	rl	l
	rl	h
	djnz	__fusi_lsh_lp
	ret

__fusi_rsh:
	neg
	ld	b, a
__fusi_rsh_lp:
	srl	h
	rr	l
	rr	d
	rr	e
	djnz	__fusi_rsh_lp
__fusi_done:
	ret

__fusi_zero:
	ld	hl, #0
	ld	d, h
	ld	e, l
	ret

__fusi_overflow:
	ld	hl, #0xFFFF
	ld	de, #0xFFFF
	ret

;===------------------------------------------------------------------------===;
; ___floatsisf - Convert signed int32 to float
;
; Input:  HLDE = signed int32
; Output: HLDE = float
;
; Algorithm:
;   1. If 0 → return +0.0
;   2. Save sign, take absolute value
;   3. Find highest set bit → determines exponent
;   4. Shift mantissa to position implicit bit at bit 23
