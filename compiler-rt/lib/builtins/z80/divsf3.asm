; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___divsf3_fast
	.globl ___divsf3
	.globl __div_a_ok
	.globl __div_b_ok
	.globl __div_b_nz
	.globl __div_unpack_a
	.globl __div_a_nrm
	.globl __div_a_up
	.globl __div_b_nrm
	.globl __div_b_up
	.globl __div_exp_pos
	.globl __div_exp_denorm
	.globl __div_exp_neg
	.globl __div_pre_norm
	.globl __div_no_pre
	.globl __div_loop_start
	.globl __div_lp
	.globl __div_do_sub
	.globl __div_no_sub
	.globl __div_q_shift
	.globl __div_guard1
	.globl __div_rnd_up
	.globl __div_no_rnd
	.globl __div_normal_pack
	.globl __div_pk0
	.globl __div_denorm_s1
	.globl __div_denorm_check
	.globl __div_denorm_neg
	.globl __div_denorm_zero
	.globl __div_denorm_shift
	.globl __div_denorm_lp
	.globl __div_done
	.globl __div_nan
	.globl __div_inf
	.globl __div_zero

___divsf3_fast:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	-1(ix), h
	ld	-2(ix), l
	ld	-3(ix), d
	ld	-4(ix), e
	ld	hl, #-14
	add	hl, sp
	ld	sp, hl
	ld	a, -1(ix)
	xor	7(ix)
	and	#0x80
	ld	-5(ix), a
	jp	__div_unpack_a

;===------------------------------------------------------------------------===;
; ___divsf3 - IEEE 754 single-precision floating point division
;
; Input:  HLDE = dividend (a), stack = divisor (b)
; Output: HLDE = a / b
; Frame layout (14 bytes):
;   -1(ix)=H_a  -2(ix)=L_a  -3(ix)=D_a  -4(ix)=E_a
;   -5(ix)=result sign  -6(ix)=result exponent
;   -7(ix)=overflow flag (carry from remainder shift)
;   -8..-10(ix)=dividend/remainder mantissa (3 bytes)
;   -11..-13(ix)=divisor mantissa (3 bytes)
;===------------------------------------------------------------------------===;
___divsf3:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	-1(ix), h
	ld	-2(ix), l
	ld	-3(ix), d
	ld	-4(ix), e
	ld	hl, #-14
	add	hl, sp
	ld	sp, hl

	; Result sign = XOR of both signs
	ld	a, -1(ix)
	xor	7(ix)
	and	#0x80
	ld	-5(ix), a

	; --- Special cases ---
	; Check a for NaN/Inf
	ld	a, -1(ix)
	and	#0x7F
	cp	#0x7F
	jr	nz, __div_a_ok
	bit	7, -2(ix)
	jr	z, __div_a_ok
	ld	a, -2(ix)
	and	#0x7F
	or	-3(ix)
	or	-4(ix)
	jp	nz, __div_nan		; a is NaN
	; a is Inf: inf/inf=NaN, inf/x=inf
	ld	a, 7(ix)
	and	#0x7F
	cp	#0x7F
	jp	nz, __div_inf
	bit	7, 6(ix)
	jp	z, __div_inf
	jp	__div_nan		; inf/inf = NaN
__div_a_ok:
	; Check b for NaN/Inf
	ld	a, 7(ix)
	and	#0x7F
	cp	#0x7F
	jr	nz, __div_b_ok
	bit	7, 6(ix)
	jr	z, __div_b_ok
	ld	a, 6(ix)
	and	#0x7F
	or	5(ix)
	or	4(ix)
	jp	nz, __div_nan		; b is NaN
	; b is Inf: x/inf = 0
	jp	__div_zero
__div_b_ok:
	; Check b == 0
	ld	a, 7(ix)
	and	#0x7F
	or	6(ix)
	or	5(ix)
	or	4(ix)
	jr	nz, __div_b_nz
	; b == 0: check if a == 0 too
	ld	a, -1(ix)
	and	#0x7F
	or	-2(ix)
	or	-3(ix)
	or	-4(ix)
	jp	z, __div_nan		; 0/0 = NaN
	jp	__div_inf		; x/0 = inf
__div_b_nz:
	; Check a == 0
	ld	a, -1(ix)
	and	#0x7F
	or	-2(ix)
	or	-3(ix)
	or	-4(ix)
	jp	z, __div_zero		; 0/x = 0
__div_unpack_a:

	; --- Unpack a (dividend) ---
	ld	a, -1(ix)
	add	a, a
	ld	b, a
	ld	a, -2(ix)
	rlca
	and	#1
	or	b
	ld	c, a		; C = exp_a
	or	a
	jr	nz, __div_a_nrm
	; Denormal a
	ld	a, -2(ix)
	and	#0x7F
	ld	-8(ix), a
	ld	c, #1
	jr	__div_a_up
__div_a_nrm:
	ld	a, -2(ix)
	or	#0x80
	ld	-8(ix), a
__div_a_up:
	ld	a, -3(ix)
	ld	-9(ix), a
	ld	a, -4(ix)
	ld	-10(ix), a
	ld	-6(ix), c	; exp_a saved

	; --- Unpack b (divisor) ---
	ld	a, 7(ix)
	add	a, a
	ld	b, a
	ld	a, 6(ix)
	rlca
	and	#1
	or	b
	ld	c, a		; C = exp_b
	or	a
	jr	nz, __div_b_nrm
	; Denormal b
	ld	a, 6(ix)
	and	#0x7F
	ld	-11(ix), a
	ld	c, #1
	jr	__div_b_up
__div_b_nrm:
	ld	a, 6(ix)
	or	#0x80
	ld	-11(ix), a
__div_b_up:
	ld	a, 5(ix)
	ld	-12(ix), a
	ld	a, 4(ix)
	ld	-13(ix), a

	; --- Calculate exponent ---
	; result_exp = exp_a - exp_b + 127
	ld	a, -6(ix)
	sub	c		; C still has exp_b
	ld	l, a
	ld	h, #0
	bit	7, a
	jr	z, __div_exp_pos
	ld	h, #0xFF	; sign-extend negative
__div_exp_pos:
	ld	de, #127
	add	hl, de		; HL = exp_a - exp_b + 127
	bit	7, h
	jr	nz, __div_exp_neg
	; Non-negative: check overflow
	ld	a, h
	or	a
	jp	nz, __div_inf	; overflow (> 255)
	ld	a, l
	cp	#255
	jp	nc, __div_inf
	or	a
	jr	z, __div_exp_denorm
	ld	-6(ix), a	; store exp (1..254)
	jr	__div_pre_norm
__div_exp_denorm:
	; exp = 0: set denorm flag in sign byte, store exp=1
	ld	a, -5(ix)
	or	#0x41		; bit 6 (denorm flag) + base_shift=1
	ld	-5(ix), a
	ld	a, #1
	ld	-6(ix), a
	jr	__div_pre_norm
__div_exp_neg:
	; Negative exponent: check if salvageable for subnormal
	ld	a, h
	inc	a
	jp	nz, __div_zero	; H != 0xFF → exp < -256 → zero
	ld	a, l
	cp	#0xE9
	jp	c, __div_zero	; exp < -23 → zero
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
__div_pre_norm:

	; --- Pre-normalize ---
	; If dividend < divisor, shift dividend left by 1 and decrement exponent.
	; This ensures the first quotient bit is always 1.
	ld	a, -10(ix)
	sub	-13(ix)
	ld	a, -9(ix)
	sbc	a, -12(ix)
	ld	a, -8(ix)
	sbc	a, -11(ix)
	jr	nc, __div_no_pre	; dividend >= divisor
	; dividend < divisor: shift left (carry from shift saved as overflow flag)
	sla	-10(ix)
	rl	-9(ix)
	rl	-8(ix)
	ld	a, #0		; don't use xor a — it clears carry!
	adc	a, #0
	ld	-7(ix), a	; save carry as initial overflow flag
	dec	-6(ix)		; exp >= 0 always (min: 1→0)
	jr	__div_loop_start
__div_no_pre:
	xor	a
	ld	-7(ix), a	; clear overflow flag

	; --- 24-bit restoring division ---
	; Dividend/remainder: -8..-10(ix) (24 bits)
	; Divisor: -11..-13(ix) (24 bits, fixed)
	; Quotient: L:D:E (24 bits, shifted left, new bit into E[0])
	; -7(ix) = overflow flag from remainder shift
__div_loop_start:
	ld	l, #0
	ld	d, l
	ld	e, l
	ld	b, #24
__div_lp:
	; Check overflow flag from previous remainder shift
	ld	a, -7(ix)
	or	a
	jr	nz, __div_do_sub	; carry was set → remainder >= divisor
	; Compare remainder >= divisor (non-destructive)
	ld	a, -10(ix)
	sub	-13(ix)
	ld	a, -9(ix)
	sbc	a, -12(ix)
	ld	a, -8(ix)
	sbc	a, -11(ix)
	jr	c, __div_no_sub
__div_do_sub:
	; Remainder >= divisor: subtract and set quotient bit
	ld	a, -10(ix)
	sub	-13(ix)
	ld	-10(ix), a
	ld	a, -9(ix)
	sbc	a, -12(ix)
	ld	-9(ix), a
	ld	a, -8(ix)
	sbc	a, -11(ix)
	ld	-8(ix), a
	scf			; quotient bit = 1
	jr	__div_q_shift
__div_no_sub:
	or	a		; clear carry (quotient bit = 0)
__div_q_shift:
	; Shift quotient left, carry → E[0]
	rl	e
	rl	d
	rl	l
	; Shift remainder left, save carry in overflow flag
	sla	-10(ix)
	rl	-9(ix)
	rl	-8(ix)
	ld	a, #0		; don't use xor a — it clears carry!
	adc	a, #0
	ld	-7(ix), a	; 1 if carry out, 0 otherwise
	djnz	__div_lp

	; --- After 24 iterations ---
	; L:D:E = 24-bit quotient with MSB at L[7] (guaranteed by pre-normalization)
	; Remainder in -8..-10(ix), overflow flag in -7(ix)
	; Get guard bit for rounding: one more compare
	ld	a, -7(ix)
	or	a
	jr	nz, __div_guard1
	ld	a, -10(ix)
	sub	-13(ix)
	ld	a, -9(ix)
	sbc	a, -12(ix)
	ld	a, -8(ix)
	sbc	a, -11(ix)
	jr	c, __div_no_rnd		; guard=0, no rounding
__div_guard1:
	; Guard bit = 1. Subtract to get sticky remainder.
	ld	a, -10(ix)
	sub	-13(ix)
	ld	-10(ix), a
	ld	a, -9(ix)
	sbc	a, -12(ix)
	ld	-9(ix), a
	ld	a, -8(ix)
	sbc	a, -11(ix)
	ld	-8(ix), a
	; Sticky = (remaining remainder != 0)
	ld	a, -8(ix)
	or	-9(ix)
	or	-10(ix)
	jr	nz, __div_rnd_up	; guard=1, sticky=1 → round up
	; guard=1, sticky=0 → ties-to-even: check E[0]
	bit	0, e
	jr	z, __div_no_rnd		; even → don't round
__div_rnd_up:
	inc	e
	jr	nz, __div_no_rnd
	inc	d
	jr	nz, __div_no_rnd
	inc	l
	jr	nz, __div_no_rnd
	; Mantissa overflowed to 0: set implicit bit, bump exponent
	ld	l, #0x80
	inc	-6(ix)
	ld	a, -6(ix)
	cp	#255
	jp	z, __div_inf
__div_no_rnd:
	; --- Pack result ---
	; L:D:E = significand with L[7] = implicit 1
	; -6(ix) = exponent, -5(ix) = sign
	bit	6, -5(ix)	; check denorm flag
	jr	nz, __div_denorm_check
	; No denorm flag: check if exp became 0
	ld	a, -6(ix)
	or	a
	jr	z, __div_denorm_s1
__div_normal_pack:
	; A = exponent (1-254)
	ld	b, a
	ld	a, l
	and	#0x7F
	bit	0, b
	jr	z, __div_pk0
	or	#0x80		; exp bit 0 into L[7]
__div_pk0:
	ld	l, a
	ld	a, b
	srl	a		; exp >> 1
	ld	b, a		; save exp>>1
	ld	a, -5(ix)
	and	#0x80		; mask sign only
	or	b
	ld	h, a
	jr	__div_done
__div_denorm_s1:
	; Simple denorm: shift=1 (exp was 1, pre-norm decremented to 0)
	srl	l
	rr	d
	rr	e
	ld	a, -5(ix)
	and	#0x80
	ld	h, a
	jr	__div_done
__div_denorm_check:
	; Denorm flag set: compute actual_exp = exp - base_shift
	ld	a, -5(ix)
	and	#0x3F		; base_shift
	ld	c, a		; C = base_shift
	ld	a, -6(ix)	; exp
	sub	c		; A = actual_exp
	bit	7, a
	jr	nz, __div_denorm_neg
	or	a
	jr	z, __div_denorm_zero
	; actual_exp > 0: normal after all
	jr	__div_normal_pack
__div_denorm_neg:
	; actual_exp < 0: shift = 1 - actual_exp
	ld	b, a
	ld	a, #1
	sub	b
	jr	__div_denorm_shift
__div_denorm_zero:
	ld	a, #1
__div_denorm_shift:
	ld	b, a		; B = shift count
__div_denorm_lp:
	srl	l
	rr	d
	rr	e
	dec	b
	jr	nz, __div_denorm_lp
	; Pack as subnormal (exp=0)
	ld	a, -5(ix)
	and	#0x80		; sign only
	ld	h, a
__div_done:
	ld	sp, ix
	pop	ix
	pop	bc		; return address (BC safe: not in HLDE return)
	inc	sp
	inc	sp
	inc	sp
	inc	sp		; callee-cleanup: skip 4 bytes of stack args
	push	bc
	ret
__div_nan:
	ld	hl, #0x7FC0
	ld	de, #0
	jr	__div_done
__div_inf:
	ld	a, -5(ix)
	or	#0x7F
	ld	h, a
	ld	l, #0x80
	ld	d, #0
	ld	e, #0
	jr	__div_done
__div_zero:
	ld	a, -5(ix)
	ld	h, a
	ld	l, #0
	ld	d, l
	ld	e, l
	jr	__div_done

;===------------------------------------------------------------------------===;
; ___extendhfsf2 - Convert IEEE 754 half-precision (f16) to single (f32)
;
; Input:  HL = f16 (H=SEEEEE MM, L=MMMM MMMM)
; Output: HLDE = f32
;
; f16: 1 sign + 5 exp (bias 15) + 10 mantissa
; f32: 1 sign + 8 exp (bias 127) + 23 mantissa
; Normal: f32_exp = f16_exp + 112, f32_mant = f16_mant << 13
