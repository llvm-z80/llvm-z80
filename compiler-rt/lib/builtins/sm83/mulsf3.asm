; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___mulsf3_fast
	.globl ___mulsf3
	.globl __sm_mul_a_ok
	.globl __sm_mul_b_ok
	.globl __sm_mul_unpack_a
	.globl __sm_mul_a_nrm
	.globl __sm_mul_a_up
	.globl __sm_mul_b_nrm
	.globl __sm_mul_b_up
	.globl __sm_mul_lp
	.globl __sm_mul_noadd
	.globl __sm_mul_shift
	.globl __sm_mul_exp_nc
	.globl __sm_mul_exp_denorm
	.globl __sm_mul_exp_neg
	.globl __sm_mul_norm_start
	.globl __sm_mul_no_pshift
	.globl __sm_mul_nlp
	.globl __sm_mul_shift1
	.globl __sm_mul_round
	.globl __sm_mul_rnd_up
	.globl __sm_mul_no_rnd
	.globl __sm_mul_denorm_res
	.globl __sm_mul_pack
	.globl __sm_mul_normal_pack
	.globl __sm_mul_pk0
	.globl __sm_mul_denorm_s1
	.globl __sm_mul_denorm_check
	.globl __sm_mul_denorm_neg
	.globl __sm_mul_denorm_zero
	.globl __sm_mul_denorm_shift
	.globl __sm_mul_denorm_lp
	.globl __sm_mul_done
	.globl __sm_mul_nan
	.globl __sm_mul_inf
	.globl __sm_mul_zero

___mulsf3_fast:
	push	de
	push	bc
	add	sp, #-12
	ldhl	sp, #15
	ld	a, (hl)
	ldhl	sp, #21
	xor	(hl)
	and	#0x80
	ldhl	sp, #9
	ld	(hl), a
	jp	__sm_mul_unpack_a

;===------------------------------------------------------------------------===;
; ___mulsf3 - Float multiplication (IEEE 754 compliant)
;
; Input:  DEBC = a, stack = b (SP+2..SP+5)
; Output: DEBC = a * b
;
; Stack frame after prologue (12 bytes local + 4 saved):
;   SP+0..5:  product (6 bytes, MSB at SP+0)
;   SP+6..8:  multiplier (3 bytes, MSB at SP+6)
;   SP+9:     result_sign
;   SP+10:    result_exp (also reused from exp_a)
;   SP+11:    exp_b
;   SP+12:    a_C (saved)  SP+13: a_B  SP+14: a_E  SP+15: a_D
;   SP+16:    ret_lo  SP+17: ret_hi
;   SP+18:    b_C  SP+19: b_B  SP+20: b_E  SP+21: b_D
;===------------------------------------------------------------------------===;

___mulsf3:
	push	de
	push	bc
	add	sp, #-12

	; Result sign = a_sign XOR b_sign
	ldhl	sp, #15
	ld	a, (hl)		; a_D
	ldhl	sp, #21
	xor	(hl)		; b_D
	and	#0x80
	ldhl	sp, #9
	ld	(hl), a		; result_sign

	; --- NaN/Inf check: a ---
	ldhl	sp, #15
	ld	a, (hl)		; a_D
	and	#0x7F
	cp	#0x7F
	jr	nz, __sm_mul_a_ok
	ldhl	sp, #14
	ld	a, (hl)		; a_E
	bit	7, a
	jr	z, __sm_mul_a_ok
	and	#0x7F
	jp	nz, __sm_mul_nan
	ldhl	sp, #13
	ld	a, (hl)
	or	a
	jp	nz, __sm_mul_nan
	ldhl	sp, #12
	ld	a, (hl)
	or	a
	jp	nz, __sm_mul_nan
	; a is Inf: inf*0=NaN, inf*x=inf
	ldhl	sp, #21
	ld	a, (hl)
	and	#0x7F
	ldhl	sp, #20
	or	(hl)
	ldhl	sp, #19
	or	(hl)
	ldhl	sp, #18
	or	(hl)
	jp	z, __sm_mul_nan
	jp	__sm_mul_inf
__sm_mul_a_ok:

	; --- NaN/Inf check: b ---
	ldhl	sp, #21
	ld	a, (hl)		; b_D
	and	#0x7F
	cp	#0x7F
	jr	nz, __sm_mul_b_ok
	ldhl	sp, #20
	ld	a, (hl)
	bit	7, a
	jr	z, __sm_mul_b_ok
	and	#0x7F
	jp	nz, __sm_mul_nan
	ldhl	sp, #19
	ld	a, (hl)
	or	a
	jp	nz, __sm_mul_nan
	ldhl	sp, #18
	ld	a, (hl)
	or	a
	jp	nz, __sm_mul_nan
	; b is Inf: inf*0=NaN
	ldhl	sp, #15
	ld	a, (hl)
	and	#0x7F
	ldhl	sp, #14
	or	(hl)
	ldhl	sp, #13
	or	(hl)
	ldhl	sp, #12
	or	(hl)
	jp	z, __sm_mul_nan
	jp	__sm_mul_inf
__sm_mul_b_ok:

	; --- Zero check ---
	ldhl	sp, #15
	ld	a, (hl)
	and	#0x7F
	ldhl	sp, #14
	or	(hl)
	ldhl	sp, #13
	or	(hl)
	ldhl	sp, #12
	or	(hl)
	jp	z, __sm_mul_zero
	ldhl	sp, #21
	ld	a, (hl)
	and	#0x7F
	ldhl	sp, #20
	or	(hl)
	ldhl	sp, #19
	or	(hl)
	ldhl	sp, #18
	or	(hl)
	jp	z, __sm_mul_zero
__sm_mul_unpack_a:

	; --- Unpack a → multiplicand in D:B:C ---
	ldhl	sp, #15
	ld	a, (hl)		; a_D
	add	a, a
	ld	b, a
	ldhl	sp, #14
	ld	a, (hl)		; a_E
	rlca
	and	#1
	or	b		; A = exp_a
	ld	c, a
	or	a
	jr	nz, __sm_mul_a_nrm
	; Denormal a
	ldhl	sp, #14
	ld	a, (hl)
	and	#0x7F
	ld	d, a		; D = mant_hi (no implicit bit)
	ld	c, #1		; effective exp = 1
	jr	__sm_mul_a_up
__sm_mul_a_nrm:
	ldhl	sp, #14
	ld	a, (hl)
	or	#0x80		; set implicit bit
	ld	d, a
__sm_mul_a_up:
	ldhl	sp, #10
	ld	(hl), c		; save exp_a
	ldhl	sp, #13
	ld	b, (hl)		; B = a_mant_mid
	ldhl	sp, #12
	ld	c, (hl)		; C = a_mant_lo
	; Multiplicand = D:B:C

	; --- Unpack b → multiplier on stack SP+6..8 ---
	ldhl	sp, #21
	ld	a, (hl)		; b_D
	add	a, a
	ld	e, a
	ldhl	sp, #20
	ld	a, (hl)		; b_E
	rlca
	and	#1
	or	e		; A = exp_b
	ld	e, a		; E = exp_b
	or	a
	jr	nz, __sm_mul_b_nrm
	ldhl	sp, #20
	ld	a, (hl)
	and	#0x7F
	ld	e, #1		; exp_b = 1
	jr	__sm_mul_b_up
__sm_mul_b_nrm:
	ldhl	sp, #20
	ld	a, (hl)
	or	#0x80
__sm_mul_b_up:
	; A = b_mant_hi, E = exp_b
	ldhl	sp, #6
	ld	(hl), a		; mult_hi
	ldhl	sp, #19
	ld	a, (hl)		; b_B
	ldhl	sp, #7
	ld	(hl), a		; mult_mid
	ldhl	sp, #18
	ld	a, (hl)		; b_C
	ldhl	sp, #8
	ld	(hl), a		; mult_lo
	ldhl	sp, #11
	ld	(hl), e		; save exp_b

	; --- Clear product (6 bytes at SP+0..5) ---
	ldhl	sp, #0
	xor	a
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	(hl), a

	; --- 24x24 multiply loop ---
	; D:B:C = multiplicand (registers)
	; SP+6..8 = multiplier (stack), SP+0..5 = product (stack)
	; E = loop counter
	ld	e, #24

__sm_mul_lp:
	; Shift multiplier right, LSB → carry
	ldhl	sp, #6
	srl	(hl)		; mult_hi
	inc	hl
	rr	(hl)		; mult_mid
	inc	hl
	rr	(hl)		; mult_lo
	; carry = multiplier LSB
	jr	nc, __sm_mul_noadd

	; Add multiplicand (D:B:C) to product[47:24]
	ldhl	sp, #2		; prod_3 = product[31:24]
	ld	a, (hl)
	add	a, c		; + mand_lo
	ld	(hl), a
	dec	hl		; → prod_4
	ld	a, (hl)
	adc	a, b		; + mand_mid + carry
	ld	(hl), a
	dec	hl		; → prod_5 (SP+0)
	ld	a, (hl)
	adc	a, d		; + mand_hi + carry
	ld	(hl), a
	; HL = SP+0, carry from add
	jr	__sm_mul_shift

__sm_mul_noadd:
	ldhl	sp, #0		; prod_5
	or	a		; clear carry

__sm_mul_shift:
	; Shift 48-bit product right, carry → MSB
	; HL = SP+0 (prod_5)
	rr	(hl)
	inc	hl
	rr	(hl)		; prod_4
	inc	hl
	rr	(hl)		; prod_3
	inc	hl
	rr	(hl)		; prod_2
	inc	hl
	rr	(hl)		; prod_1
	inc	hl
	rr	(hl)		; prod_0

	dec	e
	jr	nz, __sm_mul_lp

	; --- Calculate exponent ---
	; result_exp = exp_a + exp_b - 127
	ldhl	sp, #10
	ld	a, (hl)		; exp_a
	ldhl	sp, #11
	add	a, (hl)		; + exp_b
	ld	c, a
	ld	b, #0
	jr	nc, __sm_mul_exp_nc
	inc	b
__sm_mul_exp_nc:
	; BC = exp_a + exp_b, subtract 127
	ld	a, c
	sub	#127
	ld	c, a
	ld	a, b
	sbc	a, #0
	ld	b, a
	bit	7, b
	jr	nz, __sm_mul_exp_neg
	; Non-negative: check overflow
	ld	a, b
	or	a
	jp	nz, __sm_mul_inf	; exp > 255
	ld	a, c
	cp	#255
	jp	nc, __sm_mul_inf
	or	a
	jr	z, __sm_mul_exp_denorm
	ldhl	sp, #10
	ld	(hl), a		; store exp (1..254)
	jr	__sm_mul_norm_start
__sm_mul_exp_denorm:
	; exp = 0: set denorm flag in sign byte, store exp=1
	ldhl	sp, #9
	ld	a, (hl)
	or	#0x41		; bit 6 (denorm flag) + base_shift=1
	ld	(hl), a
	ld	a, #1
	ldhl	sp, #10
	ld	(hl), a
	jr	__sm_mul_norm_start
__sm_mul_exp_neg:
	; Negative exponent: check if salvageable for subnormal
	ld	a, b
	inc	a
	jp	nz, __sm_mul_zero	; exp < -256 → zero
	ld	a, c
	cp	#0xE9
	jp	c, __sm_mul_zero	; exp < -23 → zero
	; base_shift = 1 - C (2..24)
	ld	a, #1
	sub	c
	ld	b, a		; B = base_shift
	ldhl	sp, #9
	ld	a, (hl)
	or	#0x40		; set denorm flag
	or	b		; OR in base_shift
	ld	(hl), a
	ld	a, #1
	ldhl	sp, #10
	ld	(hl), a		; store exp=1
__sm_mul_norm_start:

	; --- Normalize product ---
	; Load upper 3 bytes
	ldhl	sp, #0
	ld	d, (hl)		; prod_5 (hi)
	inc	hl
	ld	b, (hl)		; prod_4 (mid)
	inc	hl
	ld	c, (hl)		; prod_3 (lo)

	bit	7, d
	jr	z, __sm_mul_no_pshift
	; MSB at bit 47: exp++
	ldhl	sp, #10
	inc	(hl)
	ld	a, (hl)
	cp	#255
	jp	z, __sm_mul_inf
	jr	__sm_mul_round

__sm_mul_no_pshift:
	bit	6, d
	jr	nz, __sm_mul_shift1
	; Normalize loop
__sm_mul_nlp:
	ldhl	sp, #10
	ld	a, (hl)
	cp	#2		; exp < 2? (exp always >= 0 now)
	jr	c, __sm_mul_denorm_res
	; Shift product left (6 bytes: stack + regs)
	ldhl	sp, #5		; prod_0 (LSB)
	sla	(hl)
	dec	hl		; prod_1
	rl	(hl)
	dec	hl		; prod_2
	rl	(hl)
	rl	c		; prod_3
	rl	b		; prod_4
	rl	d		; prod_5
	ldhl	sp, #10
	dec	(hl)
	bit	6, d
	jr	z, __sm_mul_nlp

__sm_mul_shift1:
	; Shift left 1 to put implicit bit at D[7]
	ldhl	sp, #5
	sla	(hl)
	dec	hl
	rl	(hl)
	dec	hl
	rl	(hl)
	rl	c
	rl	b
	rl	d

__sm_mul_round:
	; D:B:C = significand, D[7] = implicit 1
	; Round using prod_2 (SP+3)
	ldhl	sp, #3
	ld	a, (hl)		; prod_2
	bit	7, a
	jr	z, __sm_mul_no_rnd
	and	#0x7F
	ldhl	sp, #4
	or	(hl)		; prod_1
	ldhl	sp, #5
	or	(hl)		; prod_0
	jr	nz, __sm_mul_rnd_up
	bit	0, c
	jr	z, __sm_mul_no_rnd	; ties-to-even
__sm_mul_rnd_up:
	inc	c
	jr	nz, __sm_mul_no_rnd
	inc	b
	jr	nz, __sm_mul_no_rnd
	inc	d
	ld	a, d
	or	a
	jr	nz, __sm_mul_no_rnd
	ld	d, #0x80
	ldhl	sp, #10
	inc	(hl)
	ld	a, (hl)
	cp	#255
	jp	z, __sm_mul_inf
__sm_mul_no_rnd:
	ldhl	sp, #10
	ld	a, (hl)		; result exponent
	jr	__sm_mul_pack

__sm_mul_denorm_res:
	ldhl	sp, #10
	ld	a, (hl)		; load actual exponent (may be 0 or 1)

__sm_mul_pack:
	; A = exponent, D:B:C = significand, SP+9 = sign
	ld	e, a		; E = exp (temp save)
	ldhl	sp, #9
	bit	6, (hl)
	jr	nz, __sm_mul_denorm_check
	; No denorm flag: check if exp is 0
	ld	a, e
	or	a
	jr	z, __sm_mul_denorm_s1
__sm_mul_normal_pack:
	; A = exponent (1-254)
	ld	h, a
	ld	a, d
	and	#0x7F
	bit	0, h
	jr	z, __sm_mul_pk0
	or	#0x80
__sm_mul_pk0:
	ld	e, a		; E = packed mantissa
	ld	a, h
	srl	a
	ld	d, a		; D = exp >> 1 (temp)
	ldhl	sp, #9
	ld	a, (hl)
	and	#0x80		; mask sign only
	or	d
	ld	d, a
	jr	__sm_mul_done
__sm_mul_denorm_s1:
	; Simple denorm: shift=1
	srl	d
	rr	b
	rr	c
	ld	e, d
	ldhl	sp, #9
	ld	a, (hl)
	and	#0x80
	ld	d, a
	jr	__sm_mul_done
__sm_mul_denorm_check:
	; Denorm flag set: compute actual_exp = exp - base_shift
	ldhl	sp, #9
	ld	a, (hl)
	and	#0x3F		; base_shift
	ld	l, a		; L = base_shift
	ld	a, e		; exp from E (saved earlier)
	sub	l		; A = actual_exp
	bit	7, a
	jr	nz, __sm_mul_denorm_neg
	or	a
	jr	z, __sm_mul_denorm_zero
	; actual_exp > 0: normal after all
	jr	__sm_mul_normal_pack
__sm_mul_denorm_neg:
	; actual_exp < 0: shift = 1 - actual_exp
	ld	h, a
	ld	a, #1
	sub	h
	jr	__sm_mul_denorm_shift
__sm_mul_denorm_zero:
	ld	a, #1
__sm_mul_denorm_shift:
	ld	h, a		; H = shift count
__sm_mul_denorm_lp:
	srl	d
	rr	b
	rr	c
	dec	h
	jr	nz, __sm_mul_denorm_lp
	; Pack as subnormal (exp=0)
	ld	e, d
	ldhl	sp, #9
	ld	a, (hl)
	and	#0x80		; sign only
	ld	d, a

__sm_mul_done:
	add	sp, #16		; 12 locals + 4 saved
	pop	hl		; return address
	add	sp, #4		; callee-cleanup: skip 4 bytes of stack args
	jp	(hl)

__sm_mul_nan:
	ld	de, #0x7FC0
	ld	bc, #0
	jr	__sm_mul_done
__sm_mul_inf:
	ldhl	sp, #9
	ld	a, (hl)
	or	#0x7F
	ld	d, a
	ld	e, #0x80
	ld	b, #0
	ld	c, #0
	jr	__sm_mul_done
__sm_mul_zero:
	ldhl	sp, #9
	ld	d, (hl)		; preserve sign
	ld	e, #0
	ld	b, e
	ld	c, e
	jr	__sm_mul_done

;===------------------------------------------------------------------------===;
; ___divsf3_fast - Fast-math float division (no NaN/Inf/zero checks)
