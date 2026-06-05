; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___extendhfsf2
	.globl __h2f_exp_zero
	.globl __h2f_denorm_loop
	.globl __h2f_exp_max
	.globl __h2f_pack
	.globl __h2f_zero
	.globl ___truncsfhf2
	.globl __s2h_round_up
	.globl __s2h_no_round
	.globl __s2h_inf_nan
	.globl __s2h_inf
	.globl __s2h_overflow
	.globl __s2h_zero_denorm
	.globl __s2h_underflow
	.globl __s2h_uflow_shift
	.globl __s2h_uflow_next

___extendhfsf2:
	; Extract sign
	ld	a, h
	and	#0x80
	ld	c, a		; C = sign (bit 7)

	; Extract 5-bit exponent: (H & 0x7C) >> 2
	ld	a, h
	and	#0x7C
	rrca
	rrca			; A = 000eeeee
	ld	b, a		; B = f16 exponent

	; Extract 10-bit mantissa into DE: D=00mm, E=mmmmmmmm
	ld	a, h
	and	#0x03
	ld	d, a
	ld	e, l

	; Check exponent
	ld	a, b
	or	a
	jr	z, __h2f_exp_zero	; exp=0: zero or denormal
	cp	#31
	jr	z, __h2f_exp_max	; exp=31: inf/NaN

	; Normal: f32_exp = f16_exp + 112
	add	a, #112
	jr	__h2f_pack

__h2f_exp_zero:
	; exp=0: check mantissa
	ld	a, d
	or	e
	jr	z, __h2f_zero		; exp=0, mant=0 → ±0.0f

	; Denormal: normalize (shift mantissa left until bit 10 is set)
	ld	b, #0		; will become new exponent
__h2f_denorm_loop:
	dec	b
	sla	e
	rl	d		; DE <<= 1
	bit	2, d		; bit 10 of mantissa = bit 2 of D
	jr	z, __h2f_denorm_loop
	; Clear the implicit bit
	res	2, d
	; f32_exp = 1 + 112 + b (b is negative adjustment)
	ld	a, #113
	add	a, b
	jr	__h2f_pack

__h2f_exp_max:
	; exp=31 → inf/NaN: f32_exp = 255
	ld	a, #255

__h2f_pack:
	; Pack f32: A=f32_exp (8 bits), C=sign, DE=10-bit mantissa
	; f32 layout: H=S|exp[7:1], L=exp[0]|mant[9:3], D=mant[2:0]<<5, E=0
	srl	a		; A = exp>>1, carry = exp[0]
	or	c		; A = sign | exp[7:1]
	ld	h, a

	; Build L: exp[0] in bit 7, mantissa[9:3] in bits [6:0]
	ld	a, #0
	rra			; carry (exp[0]) → bit 7
	ld	l, a		; L = exp[0]<<7

	; mantissa[9:3] = D[1:0]<<5 | E[7:3]
	ld	a, d
	rlca
	rlca
	rlca
	rlca
	rlca
	and	#0x60		; D[1:0] in bits [6:5]
	ld	b, a
	ld	a, e
	rrca
	rrca
	rrca
	and	#0x1F		; E[7:3] in bits [4:0]
	or	b
	or	l
	ld	l, a		; L = exp[0] | mant[9:3]

	; D = mant[2:0] << 5
	ld	a, e
	and	#0x07
	rlca
	rlca
	rlca
	rlca
	rlca
	ld	d, a

	; E = 0
	ld	e, #0
	ret

__h2f_zero:
	; ±0: sign in C, rest all zeros
	ld	h, c
	ld	l, #0
	ld	d, l
	ld	e, l
	ret

;===------------------------------------------------------------------------===;
; ___truncsfhf2 - Convert IEEE 754 single (f32) to half-precision (f16)
;
; Input:  HLDE = f32 (H=SEEEEEEE, L=EMMMMMMM, D=MMMMMMMM, E=MMMMMMMM)
; Output: DE = f16
;
; f32 → f16: exp_delta = 112, mantissa shift right 13 with rounding
; Round-to-nearest-even on 13 truncated bits
;===------------------------------------------------------------------------===;
___truncsfhf2:
	; Extract sign
	ld	a, h
	and	#0x80
	ld	c, a		; C = sign (bit 7)

	; Extract 8-bit exponent
	ld	a, h
	add	a, a		; shift left, sign out
	ld	b, a		; B = exp[7:1] << 1
	ld	a, l
	rlca			; carry = L[7] = exp[0]
	ld	a, b
	rra			; A = exp[7:0]
	ld	b, a		; B = f32 exponent

	; Extract 23-bit mantissa into DLE (reuse E from input partially)
	; L[6:0] = mant[22:16], D[7:0] = mant[15:8], E[7:0] = mant[7:0]
	ld	a, l
	and	#0x7F		; clear exp bit from L
	ld	l, a		; L = mant[22:16]

	; Check exponent
	ld	a, b
	cp	#255
	jr	z, __s2h_inf_nan	; exp=255: inf/NaN
	or	a
	jr	z, __s2h_zero_denorm	; exp=0: f32 zero or denormal → f16 zero

	; Check overflow: f32_exp > 142 (127+15) → f16 overflow → ±Inf
	cp	#143
	jr	nc, __s2h_overflow

	; Check underflow: f32_exp < 113 (127-14) → f16 denormal or zero
	cp	#113
	jr	c, __s2h_underflow

	; Normal: f16_exp = f32_exp - 112
	sub	#112		; A = f16 exponent (1..30)
	ld	b, a		; B = f16_exp

	; Add implicit bit to mantissa: set bit 7 of L (= bit 23)
	set	7, l

	; Mantissa is now in L:D:E (24 bits, bit 23 = implicit 1)
	; f16 needs mant[22:13] = L[6:0]:D[7:6] (10 bits) with rounding
	; Truncated bits = D[5:0]:E[7:0] (14 bits, but guard at D[5])
	; Wait: mant >> 13 means we keep top 10 mantissa bits
	; mant[22:13] from L[6:0] (7 bits) and D[7:6] (2 bits) = 9 bits
	; But we need 10... let me re-derive.
	;
	; 23-bit mantissa: L[6:0] D[7:0] E[7:0]
	; positions:       22..16  15..8   7..0
	; f16 needs positions 22..13 (10 bits) = L[6:0]:D[7:5] (7+3=10)
	; guard bit = D[4], round+sticky = D[3:0]:E[7:0]
	;
	; Actually shift right 13: take bits [22:13]
	; [22:16] = L[6:0] = 7 bits (high part)
	; [15:13] = D[7:5] = 3 bits (low part)
	; guard = bit 12 = D[4]
	; round+sticky = D[3:0]:E[7:0]

	; Round-to-nearest-even
	; guard = D[4]
	bit	4, d
	jr	z, __s2h_no_round	; guard=0 → truncate

	; guard=1: check round+sticky (D[3:0]:E[7:0])
	ld	a, d
	and	#0x0F
	or	e
	jr	nz, __s2h_round_up	; round+sticky != 0 → round up

	; Exact tie: round to even (check LSB of result = D[5])
	bit	5, d
	jr	z, __s2h_no_round	; LSB=0, already even → truncate

__s2h_round_up:
	; Add 1 to mantissa[9:0] = L[6:0]:D[7:5]
	; Easiest: add 0x20 to D (increment bit 5), carry into L
	ld	a, d
	add	a, #0x20
	ld	d, a
	jr	nc, __s2h_no_round
	inc	l
	; Check mantissa overflow (bit 23 overflowed → bit 24 set)
	bit	7, l
	jr	z, __s2h_no_round
	; Mantissa overflow: result mantissa = 0, exp++
	res	7, l
	srl	l
	ld	a, d
	rra
	ld	d, a
	inc	b
	; Check if exp overflowed to 31 → Inf
	ld	a, b
	cp	#31
	jr	z, __s2h_overflow

__s2h_no_round:
	; Pack f16: D=sign|exp[4:0]<<2|mant[9:8], E=mant[7:0]
	; mant[9:8] = L[1:0], mant[7:0] = extract from D[7:5] shifted
	;
	; f16 mantissa[9:0] = L[6:0]:D[7:5] >> rearrange
	; mant[9:3] = L[6:0], mant[2:0] = D[7:5]
	; f16 packed: D_out = S | exp<<2 | mant[9:8]
	;             E_out = mant[7:0]
	;
	; mant[9:8] = L[1:0]
	; mant[7:3] = L[6:2]... wait, this doesn't work.
	;
	; Let me re-think. f16 mantissa bits [9:0]:
	; [9:3] = L[6:0] (from f32 mant[22:16])
	; [2:0] = D[7:5] (from f32 mant[15:13])
	;
	; f16 packed format: SEEE EEMM MMMM MMMM
	; D_out[7] = sign
	; D_out[6:2] = exp[4:0]
	; D_out[1:0] = mant[9:8] = L[1:0]
	; E_out[7:0] = mant[7:0]
	;   mant[7:3] = L[6:2]
	;   mant[2:0] = D[7:5]

	; Build E_out = mant[7:0]
	; mant[7:3] = L[6:2]
	ld	a, l
	rrca
	rrca
	and	#0xF8		; L[6:2] in bits [7:3]
	ld	e, a		; E_out partial

	; mant[2:0] = D[7:5]
	ld	a, d
	rlca
	rlca
	rlca
	and	#0x07		; D[7:5] in bits [2:0]
	or	e
	ld	e, a		; E_out = mant[7:0]

	; Build D_out = sign | exp<<2 | mant[9:8]
	ld	a, b		; f16_exp
	rlca
	rlca
	and	#0x7C		; exp<<2 in bits [6:2]
	or	c		; sign in bit 7
	ld	d, a		; D_out partial

	; mant[9:8] = L[1:0]
	ld	a, l
	and	#0x03
	or	d
	ld	d, a		; D_out = S | exp<<2 | mant[9:8]
	ret

__s2h_inf_nan:
	; f32 exp=255: inf or NaN
	; Check mantissa for NaN
	ld	a, l
	or	d
	or	e
	jr	z, __s2h_inf
	; NaN: f16 exp=31, preserve some mantissa bits (quiet NaN)
	; Set mant = 0x200 (quiet NaN bit) | truncated mantissa
	ld	d, c		; sign
	ld	a, d
	or	#0x7E		; exp=31 (0x1F<<2=0x7C) | quiet NaN bit (mant[9]=1)
	ld	d, a
	ld	e, #0
	ret
__s2h_inf:
	; ±Inf: exp=31, mant=0
	ld	a, c		; sign
	or	#0x7C		; exp=31 << 2
	ld	d, a
	ld	e, #0
	ret

__s2h_overflow:
	; Overflow → ±Inf
	ld	a, c
	or	#0x7C
	ld	d, a
	ld	e, #0
	ret

__s2h_zero_denorm:
	; f32 exp=0 → result is ±0 in f16 (f32 denormals are too small)
	ld	d, c		; sign only
	ld	e, #0
	ret

__s2h_underflow:
	; f32_exp < 113: f16 denormal or zero
	; shift amount = 113 - f32_exp + 1 (includes implicit bit shift)
	; if shift > 11 → result is zero (mantissa shifted out completely)
	ld	a, #113
	sub	b		; A = 113 - f32_exp = shift beyond implicit bit
	cp	#11
	jr	nc, __s2h_zero_denorm	; too small → ±0

	; Add implicit bit
	set	7, l		; L:D:E now has 24-bit mantissa with implicit 1

	; We need to right-shift L:D:E by (shift_amount + 3) to get 10-bit mantissa
	; But simpler: shift L:D by 'shift_amount' to get mantissa in L:D,
	; then extract top 10 bits like normal path

	; Total right shift of the 24-bit mantissa to position bit 22
	; at bit (22-shift_amount). For f16, we need bits [22:13] after
	; the original mantissa, so after shifting right by shift_amount,
	; we need bits [22-shift:13-shift].
	;
	; Simpler approach: shift L:D:E right by shift_amount positions.
	; Then the top 10 bits of the resulting mantissa are the f16 mantissa.
	; guard/round/sticky from the remaining bits.

	ld	b, a		; B = shift count
__s2h_uflow_shift:
	; Sticky bit preservation: OR shifted-out bits into E[0]
	ld	a, e
	or	a		; set sticky if E != 0
	srl	l
	rr	d
	rr	e
	jr	nz, __s2h_uflow_next
	; If the bit shifted out of E was 1, preserve sticky
	jr	nc, __s2h_uflow_next
	set	0, e
__s2h_uflow_next:
	djnz	__s2h_uflow_shift

	; Now L:D:E has the shifted mantissa (no implicit bit in f16 denormal)
	; f16_exp = 0 for denormal
	ld	b, #0

	; Rounding: same logic as normal path
	; guard = D[4], round+sticky = D[3:0]:E[7:0]
	bit	4, d
	jr	z, __s2h_no_round

	ld	a, d
	and	#0x0F
	or	e
	jr	nz, __s2h_round_up

	bit	5, d
	jr	z, __s2h_no_round
	jr	__s2h_round_up
