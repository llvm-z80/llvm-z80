; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___subsf3_fast
	.globl ___addsf3_fast
	.globl ___subsf3
	.globl ___addsf3
	.globl __sm_add_nan_a_no
	.globl __sm_add_nan_b_no
	.globl __sm_add_unpack_a
	.globl __sm_add_a_norm
	.globl __sm_add_a_done
	.globl __sm_add_b_norm
	.globl __sm_add_b_done
	.globl __sm_add_no_inf_a
	.globl __sm_add_no_inf
	.globl __sm_add_a_nz
	.globl __sm_add_b_nz
	.globl __sm_add_shb_lp
	.globl __sm_add_shb_ns
	.globl __sm_add_shift_a
	.globl __sm_add_sha_lp
	.globl __sm_add_sha_ns
	.globl __sm_add_no_shift
	.globl __sm_add_aligned
	.globl __sm_add_carry_ns
	.globl __sm_add_norm_load_e
	.globl __sm_add_sub_mant
	.globl __sm_add_sub_cmp
	.globl __sm_add_sub_bga
	.globl __sm_add_sub_eq
	.globl __sm_add_norm
	.globl __sm_add_nlp
	.globl __sm_add_denorm
	.globl __sm_add_round
	.globl __sm_add_rup
	.globl __sm_add_rup_ok
	.globl __sm_add_pack
	.globl __sm_add_pe0
	.globl __sm_add_pack_den
	.globl __sm_add_done
	.globl __sm_add_nan_yes
	.globl __sm_add_ret_a
	.globl __sm_add_ret_b
	.globl __sm_add_ret_inf
	.globl __sm_add_ret_zero

___subsf3_fast:
	ldhl	sp, #5
	ld	a, (hl)
	xor	#0x80
	ld	(hl), a
	; Fall through to ___addsf3_fast

;===------------------------------------------------------------------------===;
; ___addsf3_fast - Fast-math float addition (no NaN checks)
;===------------------------------------------------------------------------===;
___addsf3_fast:
	push	de
	push	bc
	add	sp, #-9
	jp	__sm_add_unpack_a

;===------------------------------------------------------------------------===;
; ___subsf3 - Float subtraction
;
; Input:  DEBC = a, stack = b (SP+2..SP+5)
; Output: DEBC = a - b
;
; Flips sign of b then falls through to ___addsf3.
;===------------------------------------------------------------------------===;

___subsf3:
	; Flip sign bit of b_D on the stack
	; SP+0=ret_lo, SP+1=ret_hi, SP+2=b_C, SP+3=b_B, SP+4=b_E, SP+5=b_D
	ldhl	sp, #5
	ld	a, (hl)
	xor	#0x80
	ld	(hl), a
	; Fall through to ___addsf3

;===------------------------------------------------------------------------===;
; ___addsf3 - Float addition (IEEE 754 compliant)
;
; Input:  DEBC = a, stack = b
; Output: DEBC = a + b
;
; Stack frame after prologue (13 bytes local):
;   SP+0:  round_byte
;   SP+1:  b_mant_lo       SP+2:  b_mant_mid     SP+3:  b_mant_hi
;   SP+4:  a_mant_lo       SP+5:  a_mant_mid     SP+6:  a_mant_hi
;   SP+7:  result_exp      SP+8:  result_sign
;   SP+9:  a_C (saved)     SP+10: a_B
;   SP+11: a_E (saved)     SP+12: a_D
;   SP+13: ret_lo          SP+14: ret_hi
;   SP+15: b_C (arg)       SP+16: b_B     SP+17: b_E     SP+18: b_D
;===------------------------------------------------------------------------===;

___addsf3:
	push	de		; save a_D, a_E
	push	bc		; save a_B, a_C
	add	sp, #-9		; allocate locals (round, mants, exp, sign)

	; --- NaN check: a ---
	ldhl	sp, #12
	ld	a, (hl)		; a_D
	and	#0x7F
	cp	#0x7F
	jr	nz, __sm_add_nan_a_no
	ldhl	sp, #11
	ld	a, (hl)		; a_E
	bit	7, a
	jr	z, __sm_add_nan_a_no
	and	#0x7F
	jp	nz, __sm_add_nan_yes
	ldhl	sp, #10
	ld	a, (hl)		; a_B
	or	a
	jp	nz, __sm_add_nan_yes
	ldhl	sp, #9
	ld	a, (hl)		; a_C
	or	a
	jp	nz, __sm_add_nan_yes
__sm_add_nan_a_no:

	; --- NaN check: b ---
	ldhl	sp, #18
	ld	a, (hl)		; b_D
	and	#0x7F
	cp	#0x7F
	jr	nz, __sm_add_nan_b_no
	ldhl	sp, #17
	ld	a, (hl)		; b_E
	bit	7, a
	jr	z, __sm_add_nan_b_no
	and	#0x7F
	jp	nz, __sm_add_nan_yes
	ldhl	sp, #16
	ld	a, (hl)		; b_B
	or	a
	jp	nz, __sm_add_nan_yes
	ldhl	sp, #15
	ld	a, (hl)		; b_C
	or	a
	jp	nz, __sm_add_nan_yes
__sm_add_nan_b_no:
__sm_add_unpack_a:

	; --- Unpack a ---
	; exp_a = a_D[6:0]<<1 | a_E[7]
	ldhl	sp, #12
	ld	a, (hl)		; a_D
	add	a, a		; exp[7:1] << 1
	ld	b, a
	ldhl	sp, #11
	ld	a, (hl)		; a_E
	rlca
	and	#1
	or	b		; A = exp_a
	ld	c, a		; C = exp_a
	or	a
	jr	nz, __sm_add_a_norm
	; Denormal a: no implicit bit, effective exp = 1
	ldhl	sp, #9
	ld	c, (hl)		; a_C
	inc	hl
	ld	b, (hl)		; a_B (SP+10)
	inc	hl
	ld	a, (hl)		; a_E (SP+11)
	and	#0x7F
	ldhl	sp, #4
	ld	(hl), c		; a_mant_lo
	inc	hl
	ld	(hl), b		; a_mant_mid
	inc	hl
	ld	(hl), a		; a_mant_hi
	ld	c, #1		; effective exp = 1
	jr	__sm_add_a_done
__sm_add_a_norm:
	; Normal a: set implicit 1
	ldhl	sp, #9
	ld	c, (hl)		; a_C
	inc	hl
	ld	b, (hl)		; a_B (SP+10)
	inc	hl
	ld	a, (hl)		; a_E (SP+11)
	or	#0x80		; set implicit bit
	ldhl	sp, #4
	ld	(hl), c		; a_mant_lo
	inc	hl
	ld	(hl), b		; a_mant_mid
	inc	hl
	ld	(hl), a		; a_mant_hi
	; Restore C = exp_a
	ldhl	sp, #12
	ld	a, (hl)		; a_D
	add	a, a
	ld	b, a
	ldhl	sp, #11
	ld	a, (hl)		; a_E
	rlca
	and	#1
	or	b
	ld	c, a		; C = exp_a
__sm_add_a_done:

	; --- Unpack b ---
	; exp_b = b_D[6:0]<<1 | b_E[7]
	ldhl	sp, #18
	ld	a, (hl)		; b_D
	add	a, a
	ld	b, a
	ldhl	sp, #17
	ld	a, (hl)		; b_E
	rlca
	and	#1
	or	b
	ld	b, a		; B = exp_b
	or	a
	jr	nz, __sm_add_b_norm
	; Denormal b
	ldhl	sp, #15
	ld	e, (hl)		; b_C
	inc	hl
	ld	d, (hl)		; b_B (SP+16)
	inc	hl
	ld	a, (hl)		; b_E (SP+17)
	and	#0x7F
	ldhl	sp, #1
	ld	(hl), e		; b_mant_lo
	inc	hl
	ld	(hl), d		; b_mant_mid
	inc	hl
	ld	(hl), a		; b_mant_hi
	ld	b, #1
	jr	__sm_add_b_done
__sm_add_b_norm:
	ldhl	sp, #15
	ld	e, (hl)		; b_C
	inc	hl
	ld	d, (hl)		; b_B (SP+16)
	inc	hl
	ld	a, (hl)		; b_E (SP+17)
	or	#0x80
	ldhl	sp, #1
	ld	(hl), e		; b_mant_lo
	inc	hl
	ld	(hl), d		; b_mant_mid
	inc	hl
	ld	(hl), a		; b_mant_hi
	; Restore B = exp_b
	ldhl	sp, #18
	ld	a, (hl)
	add	a, a
	ld	d, a
	ldhl	sp, #17
	ld	a, (hl)
	rlca
	and	#1
	or	d
	ld	b, a		; B = exp_b
__sm_add_b_done:
	; B = exp_b, C = exp_a

	; --- Handle infinity ---
	ld	a, c
	cp	#255
	jr	nz, __sm_add_no_inf_a
	; a is infinity
	ld	a, b
	cp	#255
	jp	nz, __sm_add_ret_a	; inf + finite = inf
	; Both infinity
	ldhl	sp, #12
	ld	a, (hl)		; a_D
	ldhl	sp, #18
	xor	(hl)		; b_D
	bit	7, a
	jp	nz, __sm_add_nan_yes	; inf - inf = NaN
	jp	__sm_add_ret_a
__sm_add_no_inf_a:
	ld	a, b
	cp	#255
	jr	nz, __sm_add_no_inf
	jp	__sm_add_ret_b		; b is inf
__sm_add_no_inf:

	; --- Handle zero ---
	ldhl	sp, #6
	ld	a, (hl)		; a_mant_hi
	dec	hl
	or	(hl)		; a_mant_mid (SP+5)
	dec	hl
	or	(hl)		; a_mant_lo (SP+4)
	jr	nz, __sm_add_a_nz
	jp	__sm_add_ret_b
__sm_add_a_nz:
	ldhl	sp, #3
	ld	a, (hl)		; b_mant_hi
	dec	hl
	or	(hl)		; b_mant_mid (SP+2)
	dec	hl
	or	(hl)		; b_mant_lo (SP+1)
	jr	nz, __sm_add_b_nz
	jp	__sm_add_ret_a
__sm_add_b_nz:
	; Clear round byte
	ldhl	sp, #0
	ld	(hl), #0

	; --- Align mantissas ---
	; B = exp_b, C = exp_a
	ld	a, c
	sub	b		; A = exp_a - exp_b
	jr	z, __sm_add_no_shift
	jr	c, __sm_add_shift_a

	; exp_a > exp_b: shift b mantissa right
	ldhl	sp, #7
	ld	(hl), c		; result_exp = exp_a
	cp	#25
	jp	nc, __sm_add_ret_a	; b vanishes

	ld	b, a		; B = shift count
	; Load b_mant for shifting
	ldhl	sp, #1
	ld	c, (hl)		; b_mant_lo
	inc	hl
	ld	e, (hl)		; b_mant_mid (SP+2)
	inc	hl
	ld	d, (hl)		; b_mant_hi (SP+3)
	xor	a		; A = 0 (round accumulator)
__sm_add_shb_lp:
	srl	d
	rr	e
	rr	c
	rr	a		; shifted-out bits → round byte
	jr	nc, __sm_add_shb_ns
	or	#1		; sticky
__sm_add_shb_ns:
	dec	b
	jr	nz, __sm_add_shb_lp
	; Store shifted b_mant back
	ldhl	sp, #3
	ld	(hl), d		; b_mant_hi
	dec	hl
	ld	(hl), e		; b_mant_mid (SP+2)
	dec	hl
	ld	(hl), c		; b_mant_lo (SP+1)
	dec	hl
	ld	(hl), a		; round byte (SP+0)
	jr	__sm_add_aligned

__sm_add_shift_a:
	; exp_b > exp_a: shift a mantissa right
	ldhl	sp, #7
	ld	(hl), b		; result_exp = exp_b
	ld	a, b
	sub	c		; A = exp_b - exp_a
	cp	#25
	jp	nc, __sm_add_ret_b	; a vanishes

	ld	b, a		; B = shift count
	ldhl	sp, #4
	ld	c, (hl)		; a_mant_lo
	inc	hl
	ld	e, (hl)		; a_mant_mid (SP+5)
	inc	hl
	ld	d, (hl)		; a_mant_hi (SP+6)
	xor	a
__sm_add_sha_lp:
	srl	d
	rr	e
	rr	c
	rr	a
	jr	nc, __sm_add_sha_ns
	or	#1
__sm_add_sha_ns:
	dec	b
	jr	nz, __sm_add_sha_lp
	ldhl	sp, #6
	ld	(hl), d		; a_mant_hi
	dec	hl
	ld	(hl), e		; a_mant_mid (SP+5)
	dec	hl
	ld	(hl), c		; a_mant_lo (SP+4)
	ldhl	sp, #0
	ld	(hl), a		; round byte
	jr	__sm_add_aligned

__sm_add_no_shift:
	ldhl	sp, #7
	ld	(hl), c		; result_exp = exp_a (= exp_b)

__sm_add_aligned:
	; --- Determine operation: add or subtract mantissas ---
	ldhl	sp, #12
	ld	a, (hl)		; a_D
	ldhl	sp, #18
	xor	(hl)		; b_D
	bit	7, a
	jr	nz, __sm_add_sub_mant

	; === Same signs: add mantissas ===
	ldhl	sp, #12
	ld	a, (hl)		; a_D
	and	#0x80
	ldhl	sp, #8
	ld	(hl), a		; result_sign = sign_a

	; Add: load a_mant into D:__:C, b_mant into regs, add
	ldhl	sp, #4
	ld	c, (hl)		; a_mant_lo
	inc	hl
	ld	b, (hl)		; a_mant_mid (SP+5)
	inc	hl
	ld	d, (hl)		; a_mant_hi (SP+6)
	; Load b_mant
	ldhl	sp, #1
	ld	a, (hl)		; b_mant_lo
	add	a, c
	ld	c, a		; C = result_lo
	inc	hl
	ld	a, (hl)		; b_mant_mid (SP+2)
	adc	a, b
	ld	b, a		; B = result_mid
	inc	hl
	ld	a, (hl)		; b_mant_hi (SP+3)
	adc	a, d
	ld	d, a		; D = result_hi

	; Check carry (24-bit overflow)
	jr	nc, __sm_add_norm_load_e
	; Carry: shift right, exp++
	rr	d
	rr	b
	rr	c
	; carry = old C[0], rotate into round byte
	ldhl	sp, #0
	ld	a, (hl)		; old round byte
	rra			; carry → A[7], A[0] → carry
	jr	nc, __sm_add_carry_ns
	or	#1		; sticky
__sm_add_carry_ns:
	ld	(hl), a		; store updated round byte
	ldhl	sp, #7
	inc	(hl)		; exp++
	ld	a, (hl)
	cp	#255
	jp	z, __sm_add_ret_inf

__sm_add_norm_load_e:
	; Load round byte into E for normalization phase
	ldhl	sp, #0
	ld	e, (hl)
	jp	__sm_add_norm

__sm_add_sub_mant:
	; === Different signs: subtract mantissas ===
	; Compare magnitudes
	ldhl	sp, #6
	ld	a, (hl)		; a_mant_hi
	ldhl	sp, #3
	cp	(hl)		; vs b_mant_hi
	jr	nz, __sm_add_sub_cmp
	ldhl	sp, #5
	ld	a, (hl)		; a_mant_mid
	ldhl	sp, #2
	cp	(hl)		; vs b_mant_mid
	jr	nz, __sm_add_sub_cmp
	ldhl	sp, #4
	ld	a, (hl)		; a_mant_lo
	ldhl	sp, #1
	cp	(hl)		; vs b_mant_lo
__sm_add_sub_cmp:
	jr	z, __sm_add_sub_eq	; magnitudes equal → zero
	jr	c, __sm_add_sub_bga	; |a| < |b|

	; |a| >= |b|: result = a - b, sign = sign_a
	ldhl	sp, #12
	ld	a, (hl)		; a_D
	and	#0x80
	ldhl	sp, #8
	ld	(hl), a		; result_sign = sign_a

	; 4-byte sub: a_mant:00 - b_mant:round_byte
	; Step 0: 0 - round_byte
	ldhl	sp, #0
	xor	a
	sub	(hl)
	ld	(hl), a		; new round byte
	; carry = borrow
	push	af		; save borrow [SP-=2]

	; Step 1: a_lo - b_lo - borrow
	ldhl	sp, #6		; a_mant_lo (SP+4+2)
	ld	d, (hl)
	ldhl	sp, #3		; b_mant_lo (SP+1+2)
	ld	e, (hl)
	pop	af		; restore borrow
	ld	a, d
	sbc	a, e
	ld	c, a		; C = result_lo

	push	af		; save borrow [SP-=2]
	; Step 2: a_mid - b_mid - borrow
	ldhl	sp, #7		; a_mant_mid (SP+5+2)
	ld	d, (hl)
	ldhl	sp, #4		; b_mant_mid (SP+2+2)
	ld	e, (hl)
	pop	af
	ld	a, d
	sbc	a, e
	ld	b, a		; B = result_mid

	push	af		; save borrow [SP-=2]
	; Step 3: a_hi - b_hi - borrow
	ldhl	sp, #8		; a_mant_hi (SP+6+2)
	ld	d, (hl)
	ldhl	sp, #5		; b_mant_hi (SP+3+2)
	ld	e, (hl)
	pop	af
	ld	a, d
	sbc	a, e
	ld	d, a		; D = result_hi

	; Load round byte into E
	ldhl	sp, #0
	ld	e, (hl)
	jr	__sm_add_norm

__sm_add_sub_bga:
	; |b| > |a|: result = b - a, sign = sign_b
	ldhl	sp, #18
	ld	a, (hl)		; b_D
	and	#0x80
	ldhl	sp, #8
	ld	(hl), a		; result_sign = sign_b

	; 4-byte sub: b_mant:00 - a_mant:round_byte
	ldhl	sp, #0
	xor	a
	sub	(hl)
	ld	(hl), a
	push	af

	ldhl	sp, #3		; b_mant_lo (SP+1+2)
	ld	d, (hl)
	ldhl	sp, #6		; a_mant_lo (SP+4+2)
	ld	e, (hl)
	pop	af
	ld	a, d
	sbc	a, e
	ld	c, a

	push	af
	ldhl	sp, #4		; b_mant_mid (SP+2+2)
	ld	d, (hl)
	ldhl	sp, #7		; a_mant_mid (SP+5+2)
	ld	e, (hl)
	pop	af
	ld	a, d
	sbc	a, e
	ld	b, a

	push	af
	ldhl	sp, #5		; b_mant_hi (SP+3+2)
	ld	d, (hl)
	ldhl	sp, #8		; a_mant_hi (SP+6+2)
	ld	e, (hl)
	pop	af
	ld	a, d
	sbc	a, e
	ld	d, a

	ldhl	sp, #0
	ld	e, (hl)
	jr	__sm_add_norm

__sm_add_sub_eq:
	; Magnitudes equal, different signs → +0.0
	ld	d, #0
	ld	e, d
	ld	b, d
	ld	c, d
	jp	__sm_add_done

	; --- Normalize result ---
	; D:B:C = mantissa, E = round byte
	; SP+7 = exponent, SP+8 = sign
__sm_add_norm:
	ld	a, d
	or	b
	or	c
	jp	z, __sm_add_ret_zero

__sm_add_nlp:
	bit	7, d
	jr	nz, __sm_add_round
	ldhl	sp, #7
	ld	a, (hl)		; exponent
	cp	#2
	jr	c, __sm_add_denorm
	; Shift left: round → C → B → D
	sla	e		; round byte left, bit 7 → carry
	rl	c
	rl	b
	rl	d
	ldhl	sp, #7
	dec	(hl)		; exp--
	jr	__sm_add_nlp
__sm_add_denorm:
	ldhl	sp, #7
	ld	(hl), #0	; exp = 0
	; fall through to rounding

	; --- Round-to-nearest-even ---
__sm_add_round:
	ld	a, e		; round byte
	or	a
	jr	z, __sm_add_pack	; no rounding needed
	bit	7, a
	jr	z, __sm_add_pack	; guard=0, truncate
	and	#0x7F		; mask guard, keep round+sticky
	jr	nz, __sm_add_rup	; round+sticky != 0 → round up
	; Tie: round to even
	bit	0, c
	jr	z, __sm_add_pack	; LSB=0 (even), truncate
__sm_add_rup:
	inc	c
	jr	nz, __sm_add_rup_ok
	inc	b
	jr	nz, __sm_add_rup_ok
	inc	d
	ld	a, d
	or	a
	jr	nz, __sm_add_rup_ok
	; Full overflow: 0x000000 → 0x800000, exp++
	ld	d, #0x80
	ldhl	sp, #7
	inc	(hl)
	ld	a, (hl)
	cp	#255
	jp	z, __sm_add_ret_inf
__sm_add_rup_ok:
	; Check denormal→normal promotion
	ldhl	sp, #7
	ld	a, (hl)
	or	a
	jr	nz, __sm_add_pack	; already normal
	bit	7, d
	jr	z, __sm_add_pack	; still denormal
	inc	(hl)		; promote: exp 0 → 1

__sm_add_pack:
	; Pack: SP+8=sign, SP+7=exp, D:B:C=mantissa (D[7]=implicit)
	ldhl	sp, #7
	ld	a, (hl)		; exponent
	or	a
	jr	z, __sm_add_pack_den
	; Normal: pack
	ld	h, a		; H = exponent (temp)
	ld	a, d
	and	#0x7F		; remove implicit bit
	bit	0, h
	jr	z, __sm_add_pe0
	or	#0x80		; set exp[0]
__sm_add_pe0:
	ld	e, a		; E = exp[0]:mant[22:16]
	ld	a, h		; exponent
	srl	a		; exp >> 1
	ldhl	sp, #8
	or	(hl)		; combine with sign
	ld	d, a		; D = sign:exp[7:1]
	jr	__sm_add_done
__sm_add_pack_den:
	; Denormal: exp = 0
	ld	a, d
	and	#0x7F
	ld	e, a
	ldhl	sp, #8
	ld	d, (hl)		; D = sign
	; B and C already correct

__sm_add_done:
	add	sp, #13		; clean up locals + saved regs
	pop	hl		; return address
	add	sp, #4		; callee-cleanup: skip 4 bytes of stack args
	jp	(hl)

__sm_add_nan_yes:
	ld	de, #0x7FC0	; quiet NaN
	ld	bc, #0
	jr	__sm_add_done
__sm_add_ret_a:
	ldhl	sp, #9
	ld	c, (hl)		; a_C
	inc	hl
	ld	b, (hl)		; a_B (SP+10)
	inc	hl
	ld	e, (hl)		; a_E (SP+11)
	inc	hl
	ld	d, (hl)		; a_D (SP+12)
	jr	__sm_add_done
__sm_add_ret_b:
	ldhl	sp, #15
	ld	c, (hl)		; b_C
	inc	hl
	ld	b, (hl)		; b_B (SP+16)
	inc	hl
	ld	e, (hl)		; b_E (SP+17)
	inc	hl
	ld	d, (hl)		; b_D (SP+18)
	jr	__sm_add_done
__sm_add_ret_inf:
	ldhl	sp, #8
	ld	a, (hl)		; sign
	or	#0x7F
	ld	d, a
	ld	e, #0x80
	ld	b, #0
	ld	c, #0
	jr	__sm_add_done
__sm_add_ret_zero:
	ld	d, #0
	ld	e, d
	ld	b, d
	ld	c, d
	jr	__sm_add_done

;===------------------------------------------------------------------------===;
; ___mulsf3_fast - Fast-math float multiplication (no NaN/Inf/zero checks)
