; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___addsf3_fast
	.globl ___subsf3_fast
	.globl ___subsf3
	.globl ___addsf3
	.globl __add_nan_a_no
	.globl __add_nan_b_no
	.globl __add_unpack_a
	.globl __add_a_norm
	.globl __add_a_done
	.globl __add_b_norm
	.globl __add_b_done
	.globl __add_no_inf_a
	.globl __add_no_inf
	.globl __add_a_nz
	.globl __add_b_nz
	.globl __add_shb_lp
	.globl __add_shb_ns
	.globl __add_b_gone
	.globl __add_shift_a
	.globl __add_sha_lp
	.globl __add_sha_ns
	.globl __add_a_gone
	.globl __add_no_shift
	.globl __add_aligned
	.globl __add_carry_ns
	.globl __add_sub_mant
	.globl __add_sub_cmp
	.globl __add_sub_bga
	.globl __add_sub_eq
	.globl __add_norm
	.globl __add_nlp
	.globl __add_denorm
	.globl __add_round
	.globl __add_rup
	.globl __add_rup_ok
	.globl __add_pack
	.globl __add_pe0
	.globl __add_pack_den
	.globl __add_done
	.globl __add_ret_nan
	.globl __add_ret_a
	.globl __add_ret_b
	.globl __add_ret_inf
	.globl __add_ret_zero

;===------------------------------------------------------------------------===;
; ___subsf3_fast - Fast-math float subtraction (no NaN checks)
;
; Same as ___subsf3 but skips NaN detection by jumping directly to
; ___addsf3_fast after flipping b's sign bit.
;===------------------------------------------------------------------------===;
___subsf3_fast:
	push	hl
	ld	hl, #7
	add	hl, sp
	ld	a, (hl)
	xor	#0x80
	ld	(hl), a
	pop	hl
	; Fall through to ___addsf3_fast

;===------------------------------------------------------------------------===;
; ___addsf3_fast - Fast-math float addition (no NaN checks)
;
; Same as ___addsf3 but skips NaN detection at entry.
; Inf/zero handling is still present (after unpack).
;===------------------------------------------------------------------------===;
___addsf3_fast:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	-1(ix), h
	ld	-2(ix), l
	ld	-3(ix), d
	ld	-4(ix), e
	ld	hl, #-13
	add	hl, sp
	ld	sp, hl
	jp	__add_unpack_a

;===------------------------------------------------------------------------===;
; ___subsf3 - Float subtraction
;
; Input:  HLDE = a, stack = b
; Output: HLDE = a - b
;
; Flips sign of b then falls through to ___addsf3.
;===------------------------------------------------------------------------===;
___subsf3:
	; Flip sign bit of b on the stack.
	; Stack at entry: [ret_lo][ret_hi][b_E][b_D][b_L][b_H]
	; b_H is at SP+5 (SP+0=ret_lo).
	; We need to XOR byte at SP+5 with 0x80.
	push	hl		; save a_HL; now b_H at SP+7
	ld	hl, #7
	add	hl, sp		; HL = address of b_H
	ld	a, (hl)
	xor	#0x80
	ld	(hl), a
	pop	hl
	; Fall through to ___addsf3

;===------------------------------------------------------------------------===;
; ___addsf3 - Float addition (IEEE 754 compliant)
;
; Input:  HLDE = a, stack = b
; Output: HLDE = a + b
;
; Local variables (IX-relative, 13 bytes):
;   -1(ix): a_H    -2(ix): a_L    -3(ix): a_D    -4(ix): a_E
;   -5(ix): result sign
;   -6(ix): result exponent
;   -7(ix): a_mant_hi (with implicit bit)
;   -8(ix): a_mant_mid
;   -9(ix): a_mant_lo
;   -10(ix): b_mant_hi (with implicit bit)
;   -11(ix): b_mant_mid
;   -12(ix): b_mant_lo
;   -13(ix): round byte (guard/round/sticky for rounding)
;===------------------------------------------------------------------------===;
___addsf3:
	push	ix
	ld	ix, #0
	add	ix, sp
	; Save first argument
	ld	-1(ix), h
	ld	-2(ix), l
	ld	-3(ix), d
	ld	-4(ix), e
	; Allocate 9 more bytes (-5 to -13)
	; -13(ix) = round byte (guard/round/sticky for rounding)
	ld	hl, #-13
	add	hl, sp
	ld	sp, hl

	; --- NaN check ---
	; Check a for NaN
	ld	a, -1(ix)
	and	#0x7F
	cp	#0x7F
	jr	nz, __add_nan_a_no
	bit	7, -2(ix)
	jr	z, __add_nan_a_no
	ld	a, -2(ix)
	and	#0x7F
	or	-3(ix)
	or	-4(ix)
	jp	nz, __add_ret_nan
__add_nan_a_no:
	; Check b for NaN
	ld	a, 7(ix)
	and	#0x7F
	cp	#0x7F
	jr	nz, __add_nan_b_no
	bit	7, 6(ix)
	jr	z, __add_nan_b_no
	ld	a, 6(ix)
	and	#0x7F
	or	5(ix)
	or	4(ix)
	jp	nz, __add_ret_nan
__add_nan_b_no:
__add_unpack_a:

	; --- Unpack a ---
	ld	a, -1(ix)
	add	a, a		; exp[7:1] << 1
	ld	b, a
	ld	a, -2(ix)
	rlca
	and	#1
	or	b		; A = exp_a
	ld	c, a		; C = exp_a
	or	a
	jr	nz, __add_a_norm
	; Denormal a: implicit bit = 0, effective exp = 1
	ld	a, -2(ix)
	and	#0x7F
	ld	-7(ix), a
	ld	a, -3(ix)
	ld	-8(ix), a
	ld	a, -4(ix)
	ld	-9(ix), a
	ld	c, #1
	jr	__add_a_done
__add_a_norm:
	ld	a, -2(ix)
	or	#0x80		; set implicit 1
	ld	-7(ix), a
	ld	a, -3(ix)
	ld	-8(ix), a
	ld	a, -4(ix)
	ld	-9(ix), a
__add_a_done:

	; --- Unpack b ---
	ld	a, 7(ix)
	add	a, a
	ld	b, a
	ld	a, 6(ix)
	rlca
	and	#1
	or	b		; A = exp_b
	ld	b, a		; B = exp_b
	or	a
	jr	nz, __add_b_norm
	; Denormal b
	ld	a, 6(ix)
	and	#0x7F
	ld	-10(ix), a
	ld	a, 5(ix)
	ld	-11(ix), a
	ld	a, 4(ix)
	ld	-12(ix), a
	ld	b, #1
	jr	__add_b_done
__add_b_norm:
	ld	a, 6(ix)
	or	#0x80
	ld	-10(ix), a
	ld	a, 5(ix)
	ld	-11(ix), a
	ld	a, 4(ix)
	ld	-12(ix), a
__add_b_done:
	; C = exp_a, B = exp_b

	; --- Handle infinity ---
	ld	a, c
	cp	#255
	jr	nz, __add_no_inf_a
	; a is infinity
	ld	a, b
	cp	#255
	jp	nz, __add_ret_a		; inf + finite = inf
	; Both infinity: same sign → inf, different sign → NaN
	ld	a, -1(ix)
	xor	7(ix)
	bit	7, a
	jp	nz, __add_ret_nan	; inf - inf = NaN
	jp	__add_ret_a
__add_no_inf_a:
	ld	a, b
	cp	#255
	jr	nz, __add_no_inf
	jp	__add_ret_b		; b is inf, a finite → return b
__add_no_inf:

	; --- Handle zero ---
	ld	a, -7(ix)
	or	-8(ix)
	or	-9(ix)
	jr	nz, __add_a_nz
	jp	__add_ret_b		; a is zero → return b
__add_a_nz:
	ld	a, -10(ix)
	or	-11(ix)
	or	-12(ix)
	jr	nz, __add_b_nz
	jp	__add_ret_a		; b is zero → return a
__add_b_nz:
	xor	a
	ld	-13(ix), a	; round byte = 0

	; --- Align mantissas ---
	; Shift smaller-exponent mantissa right by |exp_a - exp_b|
	; Result exponent = max(exp_a, exp_b)
	ld	a, c
	sub	b		; A = exp_a - exp_b
	jr	z, __add_no_shift
	jr	c, __add_shift_a

	; exp_a > exp_b: shift b mantissa right
	ld	-6(ix), c	; result_exp = exp_a
	cp	#25
	jr	nc, __add_b_gone
	ld	b, a		; shift count
	ld	l, -10(ix)
	ld	d, -11(ix)
	ld	e, -12(ix)
	ld	c, #0		; round byte accumulator
__add_shb_lp:
	srl	l
	rr	d
	rr	e
	rr	c		; shifted-out bits → round byte
	jr	nc, __add_shb_ns
	set	0, c		; sticky: preserve fallen-off bit
__add_shb_ns:
	djnz	__add_shb_lp
	ld	-10(ix), l
	ld	-11(ix), d
	ld	-12(ix), e
	ld	-13(ix), c	; save round byte
	jr	__add_aligned

__add_b_gone:
	; b completely shifted away → just return a
	jp	__add_ret_a

__add_shift_a:
	; exp_b > exp_a: shift a mantissa right
	ld	-6(ix), b	; result_exp = exp_b
	neg			; A = exp_b - exp_a
	cp	#25
	jr	nc, __add_a_gone
	ld	b, a
	ld	l, -7(ix)
	ld	d, -8(ix)
	ld	e, -9(ix)
	ld	c, #0		; round byte accumulator
__add_sha_lp:
	srl	l
	rr	d
	rr	e
	rr	c		; shifted-out bits → round byte
	jr	nc, __add_sha_ns
	set	0, c		; sticky: preserve fallen-off bit
__add_sha_ns:
	djnz	__add_sha_lp
	ld	-7(ix), l
	ld	-8(ix), d
	ld	-9(ix), e
	ld	-13(ix), c	; save round byte
	jr	__add_aligned

__add_a_gone:
	; a completely shifted away → just return b
	jp	__add_ret_b

__add_no_shift:
	; Exponents equal, no shift needed
	ld	-6(ix), c	; result_exp = exp_a (= exp_b)

__add_aligned:
	; --- Determine operation: add or subtract mantissas ---
	ld	a, -1(ix)
	xor	7(ix)
	bit	7, a
	jr	nz, __add_sub_mant

	; === Same signs: add mantissas ===
	ld	a, -1(ix)
	and	#0x80
	ld	-5(ix), a	; result_sign = sign_a

	ld	a, -9(ix)
	add	a, -12(ix)
	ld	e, a
	ld	a, -8(ix)
	adc	a, -11(ix)
	ld	d, a
	ld	a, -7(ix)
	adc	a, -10(ix)
	ld	l, a
	; Check carry (overflow from 24-bit add)
	jp	nc, __add_norm
	; Carry: shift right, exponent++
	rr	l
	rr	d
	rr	e
	; Update round byte: carry from E[0] becomes new guard
	ld	a, -13(ix)
	rra			; carry (old E[0]) → A[7], old A[0] → carry
	jr	nc, __add_carry_ns
	or	#1		; sticky: preserve fallen-off bit
__add_carry_ns:
	ld	-13(ix), a
	inc	-6(ix)
	ld	a, -6(ix)
	cp	#255
	jp	z, __add_ret_inf
	jr	__add_norm

__add_sub_mant:
	; === Different signs: subtract mantissas ===
	; Compare magnitudes to determine result sign
	ld	a, -7(ix)
	cp	-10(ix)
	jr	nz, __add_sub_cmp
	ld	a, -8(ix)
	cp	-11(ix)
	jr	nz, __add_sub_cmp
	ld	a, -9(ix)
	cp	-12(ix)
__add_sub_cmp:
	jr	z, __add_sub_eq		; equal → result is zero
	jr	c, __add_sub_bga	; a < b in magnitude

	; |a| >= |b|: result = a - b, sign = sign_a
	; 4-byte sub: a_mant:00 - b_mant:round_byte
	ld	a, -1(ix)
	and	#0x80
	ld	-5(ix), a
	xor	a
	sub	-13(ix)		; 0 - round_byte, carry if round!=0
	ld	-13(ix), a	; new round byte (borrow result)
	ld	a, -9(ix)
	sbc	a, -12(ix)
	ld	e, a
	ld	a, -8(ix)
	sbc	a, -11(ix)
	ld	d, a
	ld	a, -7(ix)
	sbc	a, -10(ix)
	ld	l, a
	jr	__add_norm

__add_sub_bga:
	; |b| > |a|: result = b - a, sign = sign_b
	; 4-byte sub: b_mant:00 - a_mant:round_byte
	ld	a, 7(ix)
	and	#0x80
	ld	-5(ix), a
	xor	a
	sub	-13(ix)		; 0 - round_byte, carry if round!=0
	ld	-13(ix), a	; new round byte (borrow result)
	ld	a, -12(ix)
	sbc	a, -9(ix)
	ld	e, a
	ld	a, -11(ix)
	sbc	a, -8(ix)
	ld	d, a
	ld	a, -10(ix)
	sbc	a, -7(ix)
	ld	l, a
	jr	__add_norm

__add_sub_eq:
	; Magnitudes equal, different signs → +0.0
	ld	hl, #0
	ld	d, h
	ld	e, l
	jp	__add_done

	; --- Normalize result ---
__add_norm:
	; L:D:E = result mantissa, -6(ix) = exponent, -5(ix) = sign
	; -13(ix) = round byte
	ld	a, l
	or	d
	or	e
	jp	z, __add_ret_zero

	; Normalize: shift left until L[7] is set
	; Shift round byte bits back into mantissa
__add_nlp:
	bit	7, l
	jr	nz, __add_round
	ld	a, -6(ix)
	cp	#2
	jr	c, __add_denorm
	sla	-13(ix)		; round byte left, bit 7 → carry
	rl	e
	rl	d
	rl	l
	dec	-6(ix)
	jr	__add_nlp
__add_denorm:
	ld	a, #0
	ld	-6(ix), a
	; fall through to rounding

	; --- Round-to-nearest-even ---
__add_round:
	ld	a, -13(ix)
	or	a
	jr	z, __add_pack		; round byte = 0, no rounding
	bit	7, a
	jr	z, __add_pack		; guard = 0, truncate
	; guard = 1: check if round up needed
	and	#0x7F			; mask off guard, keep round+sticky
	jr	nz, __add_rup		; round+sticky != 0 → round up
	; Tie: round to even (check LSB of result)
	bit	0, e
	jr	z, __add_pack		; LSB = 0 (even), truncate
__add_rup:
	; Round up: add 1 to mantissa L:D:E
	inc	e
	jr	nz, __add_rup_ok
	inc	d
	jr	nz, __add_rup_ok
	inc	l
	ld	a, l
	or	a
	jr	nz, __add_rup_ok
	; Full overflow: 0xFFFFFF+1 = 0x000000 → 0x800000, exp++
	ld	l, #0x80
	inc	-6(ix)
	ld	a, -6(ix)
	cp	#255
	jp	z, __add_ret_inf
__add_rup_ok:
	; Check denormal → normal promotion
	ld	a, -6(ix)
	or	a
	jr	nz, __add_pack		; exp != 0, already normal
	bit	7, l
	jr	z, __add_pack		; L[7]=0, still denormal
	inc	-6(ix)			; exp 0 → 1 (promote to normal)

__add_pack:
	; Pack: -5(ix)=sign, -6(ix)=exponent, L:D:E=mantissa (L[7]=implicit)
	ld	a, -6(ix)
	ld	b, a		; B = exponent
	or	a
	jr	z, __add_pack_den
	; Normal: remove implicit bit, set exp[0]
	ld	a, l
	and	#0x7F
	bit	0, b
	jr	z, __add_pe0
	or	#0x80
__add_pe0:
	ld	l, a
	ld	a, b
	srl	a
	or	-5(ix)		; combine with sign
	ld	h, a
	jr	__add_done
__add_pack_den:
	; Denormal: exponent = 0
	ld	a, l
	and	#0x7F
	ld	l, a
	ld	a, -5(ix)
	ld	h, a
__add_done:
	ld	sp, ix
	pop	ix
	pop	bc		; return address (BC safe: not in HLDE return)
	inc	sp
	inc	sp
	inc	sp
	inc	sp		; callee-cleanup: skip 4 bytes of stack args
	push	bc
	ret

__add_ret_nan:
	ld	hl, #0x7FC0	; quiet NaN
	ld	de, #0
	jr	__add_done
__add_ret_a:
	ld	h, -1(ix)
	ld	l, -2(ix)
	ld	d, -3(ix)
	ld	e, -4(ix)
	jr	__add_done
__add_ret_b:
	ld	h, 7(ix)
	ld	l, 6(ix)
	ld	d, 5(ix)
	ld	e, 4(ix)
	jr	__add_done
__add_ret_inf:
	ld	a, -5(ix)
	or	#0x7F
	ld	h, a
	ld	l, #0x80
	ld	d, #0
	ld	e, #0
	jr	__add_done
__add_ret_zero:
	ld	hl, #0
	ld	d, h
	ld	e, l
	jr	__add_done
