; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___mulsf3_fast
	.globl ___mulsf3
	.globl __mul_a_ok
	.globl __mul_b_ok
	.globl __mul_unpack_a
	.globl __mul_a_nrm
	.globl __mul_a_up
	.globl __mul_b_nrm
	.globl __mul_b_up
	.globl __mul_lp
	.globl __mul_noadd
	.globl __mul_shift
	.globl __mul_exp_nc
	.globl __mul_exp_denorm
	.globl __mul_exp_neg
	.globl __mul_norm_start
	.globl __mul_no_pshift
	.globl __mul_nlp
	.globl __mul_shift1
	.globl __mul_round
	.globl __mul_rnd_up
	.globl __mul_no_rnd
	.globl __mul_denorm_res
	.globl __mul_pack
	.globl __mul_normal_pack
	.globl __mul_pk0
	.globl __mul_denorm_s1
	.globl __mul_denorm_check
	.globl __mul_denorm_neg
	.globl __mul_denorm_zero
	.globl __mul_denorm_shift
	.globl __mul_denorm_lp
	.globl __mul_done
	.globl __mul_nan
	.globl __mul_inf
	.globl __mul_zero

; ___mulsf3_fast - Fast-math float multiplication (no NaN/Inf/zero checks)
;===------------------------------------------------------------------------===;
___mulsf3_fast:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	-1(ix), h
	ld	-2(ix), l
	ld	-3(ix), d
	ld	-4(ix), e
	ld	hl, #-16
	add	hl, sp
	ld	sp, hl
	ld	a, -1(ix)
	xor	7(ix)
	and	#0x80
	ld	-5(ix), a
	jp	__mul_unpack_a

;===------------------------------------------------------------------------===;
; ___mulsf3 - Float multiplication (IEEE 754 compliant)
;
; Input:  HLDE = a, stack = b
; Output: HLDE = a * b
;
; Local stack frame:
;   -1(ix):  a_H    -2(ix): a_L    -3(ix): a_D    -4(ix): a_E
;   -5(ix):  result_sign
;   -6(ix):  exp_a
;   -7(ix):  exp_b
;   -8(ix):  a_mant_hi    -9(ix): a_mant_mid    -10(ix): a_mant_lo
;   -11(ix)...-16(ix): product (6 bytes, MSB at -11)
;===------------------------------------------------------------------------===;
___mulsf3:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	-1(ix), h
	ld	-2(ix), l
	ld	-3(ix), d
	ld	-4(ix), e
	ld	hl, #-16
	add	hl, sp
	ld	sp, hl

	; Result sign
	ld	a, -1(ix)
	xor	7(ix)
	and	#0x80
	ld	-5(ix), a

	; --- Special cases ---
	; Check a for NaN/Inf
	ld	a, -1(ix)
	and	#0x7F
	cp	#0x7F
	jr	nz, __mul_a_ok
	bit	7, -2(ix)
	jr	z, __mul_a_ok
	ld	a, -2(ix)
	and	#0x7F
	or	-3(ix)
	or	-4(ix)
	jp	nz, __mul_nan
	; a is Inf: inf*0=NaN, inf*x=inf
	ld	a, 7(ix)
	and	#0x7F
	or	6(ix)
	or	5(ix)
	or	4(ix)
	jp	z, __mul_nan
	jp	__mul_inf
__mul_a_ok:
	; Check b for NaN/Inf
	ld	a, 7(ix)
	and	#0x7F
	cp	#0x7F
	jr	nz, __mul_b_ok
	bit	7, 6(ix)
	jr	z, __mul_b_ok
	ld	a, 6(ix)
	and	#0x7F
	or	5(ix)
	or	4(ix)
	jp	nz, __mul_nan
	; b is Inf
	ld	a, -1(ix)
	and	#0x7F
	or	-2(ix)
	or	-3(ix)
	or	-4(ix)
	jp	z, __mul_nan
	jp	__mul_inf
__mul_b_ok:
	; Check for zero
	ld	a, -1(ix)
	and	#0x7F
	or	-2(ix)
	or	-3(ix)
	or	-4(ix)
	jp	z, __mul_zero
	ld	a, 7(ix)
	and	#0x7F
	or	6(ix)
	or	5(ix)
	or	4(ix)
	jp	z, __mul_zero
__mul_unpack_a:

	; --- Unpack a ---
	ld	a, -1(ix)
	add	a, a
	ld	b, a
	ld	a, -2(ix)
	rlca
	and	#1
	or	b		; A = exp_a
	ld	c, a		; save in C
	or	a
	jr	nz, __mul_a_nrm
	; Denormal
	ld	a, -2(ix)
	and	#0x7F
	ld	-8(ix), a
	ld	c, #1
	jr	__mul_a_up
__mul_a_nrm:
	ld	a, -2(ix)
	or	#0x80
	ld	-8(ix), a
__mul_a_up:
	ld	a, -3(ix)
	ld	-9(ix), a
	ld	a, -4(ix)
	ld	-10(ix), a
	ld	-6(ix), c	; exp_a

	; --- Unpack b (into L:D:E as multiplier) ---
	ld	a, 7(ix)
	add	a, a
	ld	b, a
	ld	a, 6(ix)
	rlca
	and	#1
	or	b		; A = exp_b
	ld	c, a		; save
	or	a
	jr	nz, __mul_b_nrm
	ld	a, 6(ix)
	and	#0x7F
	ld	l, a
	ld	c, #1
	jr	__mul_b_up
__mul_b_nrm:
	ld	a, 6(ix)
	or	#0x80
	ld	l, a
__mul_b_up:
	ld	d, 5(ix)
	ld	e, 4(ix)
	ld	-7(ix), c	; exp_b

	; --- 24×24 multiply → 48-bit product ---
	; Multiplier: L:D:E, shifted right each iteration
	; Multiplicand: -8..-10(ix), added to product[47:24]
	; Product: -11..-16(ix) (6 bytes)
	xor	a
	ld	-11(ix), a
	ld	-12(ix), a
	ld	-13(ix), a
	ld	-14(ix), a
	ld	-15(ix), a
	ld	-16(ix), a
	ld	b, #24
__mul_lp:
	; Shift multiplier right, LSB → carry
	srl	l
	rr	d
	rr	e
	jr	nc, __mul_noadd
	; Add multiplicand to product[47:24]
	ld	a, -13(ix)
	add	a, -10(ix)
	ld	-13(ix), a
	ld	a, -12(ix)
	adc	a, -9(ix)
	ld	-12(ix), a
	ld	a, -11(ix)
	adc	a, -8(ix)
	ld	-11(ix), a
	jr	__mul_shift
__mul_noadd:
	or	a		; clear carry
__mul_shift:
	; Shift 48-bit product right, carry into MSB
	rr	-11(ix)
	rr	-12(ix)
	rr	-13(ix)
	rr	-14(ix)
	rr	-15(ix)
	rr	-16(ix)
	djnz	__mul_lp

	; --- Calculate exponent ---
	; result_exp = exp_a + exp_b - 127
	ld	a, -6(ix)
	add	a, -7(ix)
	ld	l, a
	ld	h, #0
	jr	nc, __mul_exp_nc
	inc	h
__mul_exp_nc:
	ld	de, #127
	or	a
	sbc	hl, de		; HL = exp_a + exp_b - 127
	bit	7, h
	jr	nz, __mul_exp_neg
	; Non-negative: check overflow
	ld	a, h
	or	a
	jp	nz, __mul_inf	; > 255
	ld	a, l
	cp	#255
	jp	nc, __mul_inf
	or	a
	jr	z, __mul_exp_denorm
	ld	-6(ix), a	; store exp (1..254)
	jr	__mul_norm_start
__mul_exp_denorm:
	; exp = 0: set denorm flag in sign byte, store exp=1
	ld	a, -5(ix)
	or	#0x41		; bit 6 (denorm flag) + base_shift=1
	ld	-5(ix), a
	ld	a, #1
	ld	-6(ix), a
	jr	__mul_norm_start
__mul_exp_neg:
	; Negative exponent: check if salvageable for subnormal
	ld	a, h
	inc	a
	jp	nz, __mul_zero	; H != 0xFF → exp < -256 → zero
	ld	a, l
	cp	#0xE9
	jp	c, __mul_zero	; exp < -23 → zero
	; base_shift = 1 - L (2..24)
	ld	a, #1
	sub	l
	ld	c, a		; C = base_shift
	ld	a, -5(ix)
	or	#0x40		; set denorm flag
	or	c		; OR in base_shift
	ld	-5(ix), a
	ld	a, #1
	ld	-6(ix), a	; store exp=1
__mul_norm_start:

	; --- Normalize product ---
	; Product in -11..-16(ix). Upper 3 bytes L:D:E = P >> 24.
	ld	l, -11(ix)
	ld	d, -12(ix)
	ld	e, -13(ix)
	; exp = exp_a + exp_b - 127.
	; If MSB at bit 47 (L[7]): exp_result = exp + 1, mantissa = L:D:E as-is
	; If MSB at bit 46 (L[6]): exp_result = exp, shift left 1 to put at L[7]

	bit	7, l
	jr	z, __mul_no_pshift
	; Product has MSB at bit 47: L[7]=1, mantissa already positioned
	; Just increment exponent
	inc	-6(ix)
	ld	a, -6(ix)
	cp	#255
	jp	z, __mul_inf
	jr	__mul_round

__mul_no_pshift:
	; MSB not at bit 47. Check bit 46 (L[6]).
	bit	6, l
	jr	nz, __mul_shift1
	; Product < 1.0 (denormal inputs): normalize loop
	; Each shift left compensated by exponent decrement
__mul_nlp:
	ld	a, -6(ix)
	cp	#2
	jr	c, __mul_denorm_res
	sla	-16(ix)
	rl	-15(ix)
	rl	-14(ix)
	rl	e
	rl	d
	rl	l
	dec	-6(ix)
	bit	6, l
	jr	z, __mul_nlp

__mul_shift1:
	; MSB at bit 46 (L[6]). Shift left 1 to position implicit bit at L[7].
	; No exponent adjustment needed (exp = exp_a + exp_b - 127 is correct).
	sla	-16(ix)
	rl	-15(ix)
	rl	-14(ix)
	rl	e
	rl	d
	rl	l

__mul_round:
	; L:D:E = significand with L[7] = implicit 1
	; Round using -14(ix) as round info
	; R = -14(ix) bit 7, S = (-14 & 0x7F | -15 | -16) != 0, G = E[0]
	ld	a, -14(ix)
	bit	7, a
	jr	z, __mul_no_rnd
	and	#0x7F
	or	-15(ix)
	or	-16(ix)
	jr	nz, __mul_rnd_up	; S != 0 → round up
	bit	0, e
	jr	z, __mul_no_rnd		; ties-to-even: G=0, don't round
__mul_rnd_up:
	inc	e
	jr	nz, __mul_no_rnd
	inc	d
	jr	nz, __mul_no_rnd
	inc	l
	jr	nz, __mul_no_rnd
	ld	l, #0x80
	inc	-6(ix)
	ld	a, -6(ix)
	cp	#255
	jp	z, __mul_inf
__mul_no_rnd:
	ld	a, -6(ix)
	jr	__mul_pack

__mul_denorm_res:
	ld	a, -6(ix)	; load actual exponent (0 or 1)
__mul_pack:
	; A = exponent, L:D:E = significand, -5(ix) = sign
	ld	c, a		; C = exp (temp save)
	bit	6, -5(ix)	; check denorm flag
	jr	nz, __mul_denorm_check
	; No denorm flag: check if exp is 0
	ld	a, c
	or	a
	jr	z, __mul_denorm_s1
__mul_normal_pack:
	; A = exponent (1-254)
	ld	b, a
	ld	a, l
	and	#0x7F
	bit	0, b
	jr	z, __mul_pk0
	or	#0x80
__mul_pk0:
	ld	l, a
	ld	a, b
	srl	a
	ld	b, a		; save exp>>1
	ld	a, -5(ix)
	and	#0x80		; mask sign only
	or	b
	ld	h, a
	jp	__mul_done
__mul_denorm_s1:
	; Simple denorm: shift=1
	srl	l
	rr	d
	rr	e
	ld	a, -5(ix)
	and	#0x80
	ld	h, a
	jp	__mul_done
__mul_denorm_check:
	; Denorm flag set: compute actual_exp = exp - base_shift
	ld	a, -5(ix)
	and	#0x3F		; base_shift
	ld	b, a		; B = base_shift
	ld	a, c		; exp from C (saved)
	sub	b		; A = actual_exp
	bit	7, a
	jr	nz, __mul_denorm_neg
	or	a
	jr	z, __mul_denorm_zero
	; actual_exp > 0: normal after all
	jr	__mul_normal_pack
__mul_denorm_neg:
	; actual_exp < 0: shift = 1 - actual_exp
	ld	b, a
	ld	a, #1
	sub	b
	jr	__mul_denorm_shift
__mul_denorm_zero:
	ld	a, #1
__mul_denorm_shift:
	ld	b, a		; B = shift count
__mul_denorm_lp:
	srl	l
	rr	d
	rr	e
	dec	b
	jr	nz, __mul_denorm_lp
	; Pack as subnormal (exp=0)
	ld	a, -5(ix)
	and	#0x80		; sign only
	ld	h, a
__mul_done:
	ld	sp, ix
	pop	ix
	pop	bc		; return address (BC safe: not in HLDE return)
	inc	sp
	inc	sp
	inc	sp
	inc	sp		; callee-cleanup: skip 4 bytes of stack args
	push	bc
	ret
__mul_nan:
	ld	hl, #0x7FC0
	ld	de, #0
	jr	__mul_done
__mul_inf:
	ld	a, -5(ix)
	or	#0x7F
	ld	h, a
	ld	l, #0x80
	ld	d, #0
	ld	e, #0
	jr	__mul_done
__mul_zero:
	ld	a, -5(ix)
	ld	h, a
	ld	l, #0
	ld	d, l
	ld	e, l
	jr	__mul_done

;===------------------------------------------------------------------------===;
; ___divsf3_fast - Fast-math float division (no NaN/Inf/zero checks)
