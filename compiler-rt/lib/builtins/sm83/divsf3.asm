; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___divsf3_fast
	.globl ___divsf3
	.globl __sm_div_a_ok
	.globl __sm_div_b_ok
	.globl __sm_div_b_nz
	.globl __sm_div_unpack_a
	.globl __sm_div_a_nrm
	.globl __sm_div_a_up
	.globl __sm_div_b_nrm
	.globl __sm_div_b_up
	.globl __sm_div_exp_denorm
	.globl __sm_div_exp_neg
	.globl __sm_div_pre_norm
	.globl __sm_div_no_pre
	.globl __sm_div_loop_start
	.globl __sm_div_lp
	.globl __sm_div_do_sub
	.globl __sm_div_do_sub_ld
	.globl __sm_div_no_sub
	.globl __sm_div_q_shift
	.globl __sm_div_guard1
	.globl __sm_div_rnd_up
	.globl __sm_div_no_rnd
	.globl __sm_div_normal_pack
	.globl __sm_div_pk0
	.globl __sm_div_denorm_s1
	.globl __sm_div_denorm_check
	.globl __sm_div_denorm_neg
	.globl __sm_div_denorm_zero
	.globl __sm_div_denorm_shift
	.globl __sm_div_denorm_lp
	.globl __sm_div_done
	.globl __sm_div_nan
	.globl __sm_div_inf
	.globl __sm_div_zero

___divsf3_fast:
	push	de
	push	bc
	add	sp, #-9
	ldhl	sp, #12
	ld	a, (hl)
	ldhl	sp, #18
	xor	(hl)
	and	#0x80
	ldhl	sp, #8
	ld	(hl), a
	jp	__sm_div_unpack_a

;===------------------------------------------------------------------------===;
; ___divsf3 - Float division (IEEE 754 compliant)
;
; Input:  DEBC = a (dividend), stack = b (divisor)
; Output: DEBC = a / b
;
; Stack frame after prologue (9 bytes local + 4 saved):
;   SP+0:  overflow flag
;   SP+1:  rem_lo    SP+2: rem_mid    SP+3: rem_hi
;   SP+4:  div_lo    SP+5: div_mid    SP+6: div_hi
;   SP+7:  result_exp
;   SP+8:  result_sign
;   SP+9:  a_C  SP+10: a_B  SP+11: a_E  SP+12: a_D
;   SP+13: ret_lo  SP+14: ret_hi
;   SP+15: b_C  SP+16: b_B  SP+17: b_E  SP+18: b_D
;===------------------------------------------------------------------------===;

___divsf3:
	push	de
	push	bc
	add	sp, #-9

	; Result sign
	ldhl	sp, #12
	ld	a, (hl)		; a_D
	ldhl	sp, #18
	xor	(hl)		; b_D
	and	#0x80
	ldhl	sp, #8
	ld	(hl), a

	; --- NaN/Inf check: a ---
	ldhl	sp, #12
	ld	a, (hl)
	and	#0x7F
	cp	#0x7F
	jr	nz, __sm_div_a_ok
	ldhl	sp, #11
	ld	a, (hl)
	bit	7, a
	jr	z, __sm_div_a_ok
	and	#0x7F
	jp	nz, __sm_div_nan
	ldhl	sp, #10
	ld	a, (hl)
	or	a
	jp	nz, __sm_div_nan
	ldhl	sp, #9
	ld	a, (hl)
	or	a
	jp	nz, __sm_div_nan
	; a is Inf: inf/inf=NaN, inf/x=inf
	ldhl	sp, #18
	ld	a, (hl)
	and	#0x7F
	cp	#0x7F
	jp	nz, __sm_div_inf
	ldhl	sp, #17
	bit	7, (hl)
	jp	z, __sm_div_inf
	jp	__sm_div_nan
__sm_div_a_ok:
	; --- NaN/Inf check: b ---
	ldhl	sp, #18
	ld	a, (hl)
	and	#0x7F
	cp	#0x7F
	jr	nz, __sm_div_b_ok
	ldhl	sp, #17
	ld	a, (hl)
	bit	7, a
	jr	z, __sm_div_b_ok
	and	#0x7F
	jp	nz, __sm_div_nan
	ldhl	sp, #16
	ld	a, (hl)
	or	a
	jp	nz, __sm_div_nan
	ldhl	sp, #15
	ld	a, (hl)
	or	a
	jp	nz, __sm_div_nan
	jp	__sm_div_zero		; x/inf = 0
__sm_div_b_ok:
	; --- Zero checks ---
	ldhl	sp, #18
	ld	a, (hl)
	and	#0x7F
	ldhl	sp, #17
	or	(hl)
	ldhl	sp, #16
	or	(hl)
	ldhl	sp, #15
	or	(hl)
	jr	nz, __sm_div_b_nz
	; b==0: 0/0=NaN, x/0=inf
	ldhl	sp, #12
	ld	a, (hl)
	and	#0x7F
	ldhl	sp, #11
	or	(hl)
	ldhl	sp, #10
	or	(hl)
	ldhl	sp, #9
	or	(hl)
	jp	z, __sm_div_nan
	jp	__sm_div_inf
__sm_div_b_nz:
	ldhl	sp, #12
	ld	a, (hl)
	and	#0x7F
	ldhl	sp, #11
	or	(hl)
	ldhl	sp, #10
	or	(hl)
	ldhl	sp, #9
	or	(hl)
	jp	z, __sm_div_zero
__sm_div_unpack_a:

	; --- Unpack a (dividend → remainder) ---
	ldhl	sp, #12
	ld	a, (hl)
	add	a, a
	ld	b, a
	ldhl	sp, #11
	ld	a, (hl)
	rlca
	and	#1
	or	b
	ld	c, a		; C = exp_a
	or	a
	jr	nz, __sm_div_a_nrm
	ldhl	sp, #11
	ld	a, (hl)
	and	#0x7F
	ld	c, #1
	jr	__sm_div_a_up
__sm_div_a_nrm:
	ldhl	sp, #11
	ld	a, (hl)
	or	#0x80
__sm_div_a_up:
	; A = a_mant_hi, C = exp_a
	ldhl	sp, #3
	ld	(hl), a		; rem_hi
	ldhl	sp, #10
	ld	a, (hl)		; a_B
	ldhl	sp, #2
	ld	(hl), a		; rem_mid
	ldhl	sp, #9
	ld	a, (hl)		; a_C
	ldhl	sp, #1
	ld	(hl), a		; rem_lo
	; Save exp_a
	ldhl	sp, #7
	ld	(hl), c

	; --- Unpack b (divisor) ---
	ldhl	sp, #18
	ld	a, (hl)
	add	a, a
	ld	b, a
	ldhl	sp, #17
	ld	a, (hl)
	rlca
	and	#1
	or	b
	ld	c, a		; C = exp_b
	or	a
	jr	nz, __sm_div_b_nrm
	ldhl	sp, #17
	ld	a, (hl)
	and	#0x7F
	ld	c, #1
	jr	__sm_div_b_up
__sm_div_b_nrm:
	ldhl	sp, #17
	ld	a, (hl)
	or	#0x80
__sm_div_b_up:
	ldhl	sp, #6
	ld	(hl), a		; div_hi
	ldhl	sp, #16
	ld	a, (hl)		; b_B
	ldhl	sp, #5
	ld	(hl), a		; div_mid
	ldhl	sp, #15
	ld	a, (hl)		; b_C
	ldhl	sp, #4
	ld	(hl), a		; div_lo
	; C = exp_b

	; --- Calculate exponent ---
	; result_exp = exp_a - exp_b + 127
	ldhl	sp, #7
	ld	a, (hl)		; exp_a
	sub	c		; - exp_b (carry = borrow)
	ld	c, a		; C = low byte
	ld	a, #0
	sbc	a, #0		; A = 0xFF if borrow, 0 if not (proper sign-extend)
	ld	b, a
	; BC = exp_a - exp_b (signed 16-bit, correct for full range)
	; Add 127
	ld	a, c
	add	a, #127
	ld	c, a
	ld	a, b
	adc	a, #0
	ld	b, a
	; BC = result exponent (signed 16-bit)
	bit	7, b
	jr	nz, __sm_div_exp_neg
	; Non-negative: check overflow
	ld	a, b
	or	a
	jp	nz, __sm_div_inf	; exp > 255
	ld	a, c
	cp	#255
	jp	nc, __sm_div_inf
	or	a
	jr	z, __sm_div_exp_denorm
	ldhl	sp, #7
	ld	(hl), a		; store exp (1..254)
	jr	__sm_div_pre_norm
__sm_div_exp_denorm:
	; exp = 0: set denorm flag in sign byte, store exp=1
	ldhl	sp, #8
	ld	a, (hl)
	or	#0x41		; bit 6 (denorm flag) + base_shift=1
	ld	(hl), a
	ld	a, #1
	ldhl	sp, #7
	ld	(hl), a
	jr	__sm_div_pre_norm
__sm_div_exp_neg:
	; Negative exponent: check if salvageable for subnormal
	ld	a, b
	inc	a
	jp	nz, __sm_div_zero	; exp < -256 → zero
	ld	a, c
	cp	#0xE9
	jp	c, __sm_div_zero	; exp < -23 → zero
	; base_shift = 1 - C (2..24)
	ld	a, #1
	sub	c
	ld	b, a		; B = base_shift
	ldhl	sp, #8
	ld	a, (hl)
	or	#0x40		; set denorm flag
	or	b		; OR in base_shift
	ld	(hl), a
	ld	a, #1
	ldhl	sp, #7
	ld	(hl), a		; store exp=1
__sm_div_pre_norm:

	; --- Pre-normalize ---
	; Compare dividend vs divisor
	ldhl	sp, #1
	ld	c, (hl)		; rem_lo
	inc	hl
	ld	b, (hl)		; rem_mid
	inc	hl
	ld	d, (hl)		; rem_hi
	inc	hl
	ld	e, (hl)		; div_lo
	inc	hl
	ld	a, (hl)		; div_mid
	inc	hl
	ld	l, (hl)		; div_hi
	ld	h, a		; H=div_mid, L=div_hi
	ld	a, c
	sub	e
	ld	a, b
	sbc	a, h
	ld	a, d
	sbc	a, l
	jr	nc, __sm_div_no_pre
	; Dividend < divisor: shift dividend left
	ldhl	sp, #1
	sla	(hl)		; rem_lo
	inc	hl
	rl	(hl)		; rem_mid
	inc	hl
	rl	(hl)		; rem_hi
	ld	a, #0
	adc	a, #0
	ldhl	sp, #0
	ld	(hl), a		; overflow flag
	ldhl	sp, #7
	dec	(hl)		; exp >= 0 always (min: 1→0)
	jr	__sm_div_loop_start
__sm_div_no_pre:
	ldhl	sp, #0
	ld	(hl), #0

	; --- 24-bit restoring division ---
	; Quotient in D:B:C (accumulated), loop counter in E
__sm_div_loop_start:
	ld	d, #0
	ld	b, d
	ld	c, d
	ld	e, #24

__sm_div_lp:
	; Save quotient and counter
	push	de		; D(quot_hi), E(counter) [SP-=2]
	push	bc		; B(quot_mid), C(quot_lo) [SP-=2]
	; SP offsets: +4 more than normal

	; Check overflow flag
	ldhl	sp, #4		; overflow flag (SP+0+4)
	ld	a, (hl)
	or	a
	jr	nz, __sm_div_do_sub_ld

	; Load rem and div
	ldhl	sp, #5		; rem_lo (SP+1+4)
	ld	c, (hl)
	inc	hl
	ld	b, (hl)		; rem_mid
	inc	hl
	ld	d, (hl)		; rem_hi
	inc	hl
	ld	e, (hl)		; div_lo
	inc	hl
	ld	a, (hl)		; div_mid
	inc	hl
	ld	l, (hl)		; div_hi
	ld	h, a

	; Compare rem >= div (non-destructive)
	ld	a, c
	sub	e
	ld	a, b
	sbc	a, h
	ld	a, d
	sbc	a, l
	jr	c, __sm_div_no_sub

__sm_div_do_sub:
	; Subtract: rem -= div
	ld	a, c
	sub	e
	ld	c, a
	ld	a, b
	sbc	a, h
	ld	b, a
	ld	a, d
	sbc	a, l
	ld	d, a
	; Store updated remainder
	ldhl	sp, #5		; rem_lo (SP+1+4)
	ld	(hl), c
	inc	hl
	ld	(hl), b
	inc	hl
	ld	(hl), d
	; Restore quotient, set carry
	pop	bc		; quot_mid:lo [SP+=2]
	pop	de		; quot_hi:counter [SP+=2]
	scf
	jr	__sm_div_q_shift

__sm_div_do_sub_ld:
	; Overflow was set: load and go to subtract
	ldhl	sp, #5
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	ld	d, (hl)
	inc	hl
	ld	e, (hl)
	inc	hl
	ld	a, (hl)
	inc	hl
	ld	l, (hl)
	ld	h, a
	jr	__sm_div_do_sub

__sm_div_no_sub:
	pop	bc
	pop	de
	or	a		; clear carry

__sm_div_q_shift:
	; Shift quotient left, carry → C[0]
	rl	c
	rl	b
	rl	d
	; Shift remainder left
	ldhl	sp, #1		; rem_lo
	sla	(hl)
	inc	hl
	rl	(hl)		; rem_mid
	inc	hl
	rl	(hl)		; rem_hi
	; Save carry as overflow flag
	ld	a, #0
	adc	a, #0
	ldhl	sp, #0
	ld	(hl), a
	; Loop
	dec	e
	jr	nz, __sm_div_lp

	; --- Guard bit for rounding ---
	; One more compare of remainder vs divisor
	ldhl	sp, #0
	ld	a, (hl)
	or	a
	jr	nz, __sm_div_guard1
	; Load rem and div for compare
	ldhl	sp, #1
	ld	e, (hl)		; rem_lo (reuse E, loop done)
	inc	hl
	ld	a, (hl)		; rem_mid
	ld	h, a		; H = rem_mid (temp)
	inc	hl
	ld	a, (hl)		; rem_hi
	; Now load div
	inc	hl
	ld	l, (hl)		; div_lo (SP+4)
	; Need div_mid and div_hi too
	push	af		; save rem_hi [SP-=2]
	ldhl	sp, #7		; div_mid (SP+5+2)
	ld	a, (hl)		; div_mid
	inc	hl
	ld	l, (hl)		; div_hi (SP+6+2=SP+8)
	; Now: E=rem_lo, H=rem_mid, stack=rem_hi
	;      L=div_hi, A=div_mid, ?? = div_lo
	; This is getting messy. Let me just use the push/pop pattern.
	pop	af		; restore rem_hi into A
	; Hmm, I lost track. Let me redo this with push/pop quotient.

	; Actually, after the loop, quotient is in D:B:C and E is 0 (loop done).
	; I can reuse E and the push/pop pattern.
	push	de
	push	bc
	; Load rem and div (offsets +4)
	ldhl	sp, #5
	ld	c, (hl)		; rem_lo
	inc	hl
	ld	b, (hl)
	inc	hl
	ld	d, (hl)		; rem_hi
	inc	hl
	ld	e, (hl)		; div_lo
	inc	hl
	ld	a, (hl)
	inc	hl
	ld	l, (hl)		; div_hi
	ld	h, a		; H=div_mid, L=div_hi
	ld	a, c
	sub	e
	ld	a, b
	sbc	a, h
	ld	a, d
	sbc	a, l
	pop	bc
	pop	de
	jr	c, __sm_div_no_rnd

__sm_div_guard1:
	; Guard=1, subtract for sticky check
	push	de
	push	bc
	ldhl	sp, #5
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	ld	d, (hl)
	inc	hl
	ld	e, (hl)
	inc	hl
	ld	a, (hl)
	inc	hl
	ld	l, (hl)
	ld	h, a
	ld	a, c
	sub	e
	ld	c, a
	ld	a, b
	sbc	a, h
	ld	b, a
	ld	a, d
	sbc	a, l
	; Sticky = (C | B | A) != 0
	or	b
	or	c
	pop	bc		; restore quotient
	pop	de
	jr	nz, __sm_div_rnd_up
	; Guard=1, sticky=0: ties-to-even
	bit	0, c
	jr	z, __sm_div_no_rnd
__sm_div_rnd_up:
	inc	c
	jr	nz, __sm_div_no_rnd
	inc	b
	jr	nz, __sm_div_no_rnd
	inc	d
	ld	a, d
	or	a
	jr	nz, __sm_div_no_rnd
	ld	d, #0x80
	ldhl	sp, #7
	inc	(hl)
	ld	a, (hl)
	cp	#255
	jp	z, __sm_div_inf
__sm_div_no_rnd:
	; --- Pack result ---
	; D:B:C = significand, D[7]=implicit 1, SP+7=exp, SP+8=sign
	ldhl	sp, #8
	bit	6, (hl)
	jr	nz, __sm_div_denorm_check
	; No denorm flag: check if exp became 0
	ldhl	sp, #7
	ld	a, (hl)
	or	a
	jr	z, __sm_div_denorm_s1
__sm_div_normal_pack:
	; A = exponent (1-254)
	ld	h, a
	ld	a, d
	and	#0x7F
	bit	0, h
	jr	z, __sm_div_pk0
	or	#0x80
__sm_div_pk0:
	ld	e, a
	ld	a, h
	srl	a
	ld	d, a		; D = exp >> 1 (temp)
	ldhl	sp, #8
	ld	a, (hl)
	and	#0x80		; mask sign only
	or	d
	ld	d, a
	jr	__sm_div_done
__sm_div_denorm_s1:
	; Simple denorm: shift=1 (exp was 1, pre-norm decremented to 0)
	srl	d
	rr	b
	rr	c
	ld	e, d
	ldhl	sp, #8
	ld	a, (hl)
	and	#0x80
	ld	d, a
	jr	__sm_div_done
__sm_div_denorm_check:
	; Denorm flag set: compute actual_exp = exp_at_SP7 - base_shift
	ldhl	sp, #8
	ld	a, (hl)
	and	#0x3F		; base_shift
	ld	e, a		; E = base_shift (temp)
	ldhl	sp, #7
	ld	a, (hl)		; exp_at_SP7
	sub	e		; A = actual_exp
	bit	7, a
	jr	nz, __sm_div_denorm_neg
	or	a
	jr	z, __sm_div_denorm_zero
	; actual_exp > 0: normal after all
	jr	__sm_div_normal_pack
__sm_div_denorm_neg:
	; actual_exp < 0: shift = 1 - actual_exp
	ld	h, a
	ld	a, #1
	sub	h
	jr	__sm_div_denorm_shift
__sm_div_denorm_zero:
	ld	a, #1
__sm_div_denorm_shift:
	ld	h, a		; H = shift count
__sm_div_denorm_lp:
	srl	d
	rr	b
	rr	c
	dec	h
	jr	nz, __sm_div_denorm_lp
	; Pack as subnormal (exp=0)
	ld	e, d
	ldhl	sp, #8
	ld	a, (hl)
	and	#0x80		; sign only
	ld	d, a

__sm_div_done:
	add	sp, #13		; 9 locals + 4 saved
	pop	hl		; return address
	add	sp, #4		; callee-cleanup: skip 4 bytes of stack args
	jp	(hl)

__sm_div_nan:
	ld	de, #0x7FC0
	ld	bc, #0
	jr	__sm_div_done
__sm_div_inf:
	ldhl	sp, #8
	ld	a, (hl)
	or	#0x7F
	ld	d, a
	ld	e, #0x80
	ld	b, #0
	ld	c, #0
	jr	__sm_div_done
__sm_div_zero:
	ldhl	sp, #8
	ld	d, (hl)
	ld	e, #0
	ld	b, e
	ld	c, e
	jr	__sm_div_done

;===------------------------------------------------------------------------===;
; ___fixsfsi - Convert float to signed int32
;
; Input:  DEBC = float (D=sign+exp_hi, E=exp_lo+mant_hi, B=mant_mid, C=mant_lo)
; Output: DEBC = signed int32 (D=MSB, C=LSB)
;
; Algorithm:
;   1. Extract sign, exponent, mantissa
;   2. If exp < 127 (|value| < 1.0) → return 0
;   3. If exp >= 158 → overflow (clamp to INT32_MAX/MIN)
;   4. Set implicit bit, mantissa = 0:E:B:C (24-bit)
;   5. shift = exp - 150: positive → left shift, negative → right shift
;   6. Apply sign (negate if negative)
