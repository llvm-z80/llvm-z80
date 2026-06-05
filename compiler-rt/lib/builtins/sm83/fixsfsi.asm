; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___fixsfsi
	.globl __sm_fsi_not_nan
	.globl __sm_fsi_lsh_lp
	.globl __sm_fsi_rshift
	.globl __sm_fsi_rsh_lp
	.globl __sm_fsi_sign
	.globl __sm_fsi_zero
	.globl __sm_fsi_zero_nr
	.globl __sm_fsi_overflow
	.globl __sm_fsi_ovf_neg
	.globl ___fixunssfsi
	.globl __sm_fusi_not_nan
	.globl __sm_fusi_lsh_lp
	.globl __sm_fusi_rsh
	.globl __sm_fusi_rsh_lp
	.globl __sm_fusi_done
	.globl __sm_fusi_zero
	.globl __sm_fusi_overflow

___fixsfsi:
	; --- NaN check: exp=255 and mantissa!=0 → return 0 ---
	ld	a, d
	and	#0x7F
	cp	#0x7F
	jr	nz, __sm_fsi_not_nan
	bit	7, e
	jr	z, __sm_fsi_not_nan
	; exp bits = 0xFF, check mantissa
	ld	a, e
	and	#0x7F
	or	b
	or	c
	jr	nz, __sm_fsi_zero_nr	; NaN → return 0
__sm_fsi_not_nan:

	; Extract sign
	ld	a, d
	and	#0x80
	push	af		; save sign on stack

	; Extract exponent: D[6:0]<<1 | E[7]
	ld	a, d
	add	a, a		; A = exp[7:1] << 1
	ld	h, a
	ld	a, e
	rlca
	and	#1
	or	h		; A = full exponent
	; A = exponent
	or	a
	jr	z, __sm_fsi_zero	; exp=0 → ±0 or denormal

	; Check if |value| < 1.0 (exp < 127)
	cp	#127
	jr	c, __sm_fsi_zero

	; Check overflow: exp >= 158
	cp	#158
	jr	nc, __sm_fsi_overflow

	; Save exponent in H (HL not needed for stack access here)
	ld	h, a

	; Set implicit bit: E |= 0x80, D = 0
	ld	a, e
	or	#0x80
	ld	e, a
	ld	d, #0		; D:E:B:C = 0:(E|0x80):B:C

	; Calculate shift: exp - 150
	ld	a, h		; A = exponent
	sub	#150		; A = exp - 150 (signed)
	jr	z, __sm_fsi_sign	; no shift needed
	jr	c, __sm_fsi_rshift

	; Left shift: A = shift amount (1..7)
	ld	h, a
__sm_fsi_lsh_lp:
	sla	c
	rl	b
	rl	e
	rl	d
	dec	h
	jr	nz, __sm_fsi_lsh_lp
	jr	__sm_fsi_sign

__sm_fsi_rshift:
	; Right shift: negate A to get count (NEG = CPL+INC)
	cpl
	inc	a		; A = shift amount (1..23)
	ld	h, a
__sm_fsi_rsh_lp:
	srl	d
	rr	e
	rr	b
	rr	c
	dec	h
	jr	nz, __sm_fsi_rsh_lp

__sm_fsi_sign:
	; Apply sign
	pop	af		; restore sign
	and	#0x80
	ret	z		; positive, done

	; Negate DEBC (two's complement)
	ld	a, c
	cpl
	ld	c, a
	ld	a, b
	cpl
	ld	b, a
	ld	a, e
	cpl
	ld	e, a
	ld	a, d
	cpl
	ld	d, a
	inc	c
	ret	nz
	inc	b
	ret	nz
	inc	e
	ret	nz
	inc	d
	ret

__sm_fsi_zero:
	pop	af		; discard saved sign
__sm_fsi_zero_nr:
	ld	bc, #0
	ld	d, b
	ld	e, c
	ret

__sm_fsi_overflow:
	pop	af
	and	#0x80
	jr	nz, __sm_fsi_ovf_neg
	; Positive overflow → INT32_MAX
	ld	de, #0x7FFF
	ld	bc, #0xFFFF
	ret
__sm_fsi_ovf_neg:
	; Negative overflow → INT32_MIN
	ld	de, #0x8000
	ld	bc, #0x0000
	ret

;===------------------------------------------------------------------------===;
; ___fixunssfsi - Convert float to unsigned int32
;
; Input:  DEBC = float
; Output: DEBC = unsigned int32
;
; Same as ___fixsfsi but:
;   - Negative input → return 0
;   - No sign handling
;   - Overflow at exp >= 159
;===------------------------------------------------------------------------===;
___fixunssfsi:
	; --- NaN check ---
	ld	a, d
	and	#0x7F
	cp	#0x7F
	jr	nz, __sm_fusi_not_nan
	bit	7, e
	jr	z, __sm_fusi_not_nan
	ld	a, e
	and	#0x7F
	or	b
	or	c
	jr	nz, __sm_fusi_zero	; NaN → return 0
__sm_fusi_not_nan:

	; Check sign: negative → return 0
	bit	7, d
	jr	nz, __sm_fusi_zero

	; Extract exponent
	ld	a, d
	add	a, a
	ld	h, a
	ld	a, e
	rlca
	and	#1
	or	h		; A = exponent
	or	a
	jr	z, __sm_fusi_zero	; exp=0 → ±0 or denormal

	cp	#127
	jr	c, __sm_fusi_zero	; |value| < 1.0

	; Check overflow: exp >= 159
	cp	#159
	jr	nc, __sm_fusi_overflow

	ld	h, a		; H = exponent

	; Set implicit bit
	ld	a, e
	or	#0x80
	ld	e, a
	ld	d, #0		; D:E:B:C = 0:(E|0x80):B:C

	ld	a, h		; A = exponent
	sub	#150
	jr	z, __sm_fusi_done
	jr	c, __sm_fusi_rsh

	; Left shift
	ld	h, a
__sm_fusi_lsh_lp:
	sla	c
	rl	b
	rl	e
	rl	d
	dec	h
	jr	nz, __sm_fusi_lsh_lp
	ret

__sm_fusi_rsh:
	cpl
	inc	a		; NEG
	ld	h, a
__sm_fusi_rsh_lp:
	srl	d
	rr	e
	rr	b
	rr	c
	dec	h
	jr	nz, __sm_fusi_rsh_lp
__sm_fusi_done:
	ret

__sm_fusi_zero:
	ld	bc, #0
	ld	d, b
	ld	e, c
	ret

__sm_fusi_overflow:
	ld	de, #0xFFFF
	ld	bc, #0xFFFF
	ret

;===------------------------------------------------------------------------===;
; ___floatsisf - Convert signed int32 to float
;
; Input:  DEBC = signed int32 (D=MSB, C=LSB)
; Output: DEBC = float
;
; Algorithm:
;   1. If 0 → return +0.0
;   2. Save sign, take absolute value
;   3. Find highest set bit → determines exponent
;   4. Shift mantissa to position implicit bit at bit 23
;   5. Round-to-nearest-even using guard/round/sticky
;   6. Pack sign + exponent + mantissa
