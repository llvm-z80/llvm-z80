; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___unordsf2
	.globl __unord_ck_b
	.globl __unord_no
	.globl __unord_yes
	.globl ___gtsf2
	.globl ___gesf2
	.globl __cmpgt_nan_a_no
	.globl ___cmpsf2_fast
	.globl ___eqsf2_fast
	.globl ___nesf2_fast
	.globl ___ltsf2_fast
	.globl ___lesf2_fast
	.globl ___gtsf2_fast
	.globl ___gesf2_fast
	.globl ___cmpsf2
	.globl ___eqsf2
	.globl ___nesf2
	.globl ___ltsf2
	.globl ___lesf2
	.globl __cmp_nan_a_no
	.globl __cmp_begin
	.globl __cmp_signs
	.globl __cmp_same_sign
	.globl __cmp_mag_diff
	.globl __cmp_mag_pos
	.globl __cmp_lt
	.globl __cmp_eq
	.globl __cmp_gt
	.globl __cmp_ret

; Stack frame for two-arg float functions (after push ix; ld ix,#0; add ix,sp):
;   IX+0: saved IX (2 bytes)
;   IX+2: return address (2 bytes)
;   IX+4: arg2 byte 0 (E2, LSB)
;   IX+5: arg2 byte 1 (D2)
;   IX+6: arg2 byte 2 (L2)
;   IX+7: arg2 byte 3 (H2, MSB)
;
; IEEE 754 fully compliant:
;   - Denormal support (no flush-to-zero)
;   - NaN detection and propagation
;   - Round-to-nearest-even (banker's rounding) via guard/round/sticky
;   - Infinity arithmetic rules
;

; NaN test: exponent=255 (0xFF) AND mantissa!=0
; Exponent bits: H[6:0]=exp[7:1], L[7]=exp[0]
; For exp=255: H[6:0]=0x7F AND L[7]=1


;===------------------------------------------------------------------------===;
; ___unordsf2 - Check if either argument is NaN
;
; Input:  HLDE = a, stack = b
; Output: DE = nonzero if a or b is NaN, 0 if both ordered
;===------------------------------------------------------------------------===;
___unordsf2:
	push	ix
	ld	ix, #0
	add	ix, sp
	; Check a: exponent==255 && mantissa!=0
	ld	a, h
	and	#0x7F		; A = H[6:0]
	cp	#0x7F
	jr	nz, __unord_ck_b
	bit	7, l		; exp[0]
	jr	z, __unord_ck_b
	; exp_a == 255, check mantissa
	ld	a, l
	and	#0x7F		; mantissa[22:16]
	or	d		; mantissa[15:8]
	or	e		; mantissa[7:0]
	jr	nz, __unord_yes	; a is NaN
__unord_ck_b:
	; Check b
	ld	a, 7(ix)	; b_H
	and	#0x7F
	cp	#0x7F
	jr	nz, __unord_no
	bit	7, 6(ix)	; b_L bit 7 = exp[0]
	jr	z, __unord_no
	ld	a, 6(ix)	; b_L
	and	#0x7F
	or	5(ix)		; b_D
	or	4(ix)		; b_E
	jr	nz, __unord_yes
__unord_no:
	ld	de, #0
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp
	inc	sp
	inc	sp		; callee-cleanup: skip 4 bytes of stack args
	push	bc
	ret
__unord_yes:
	ld	de, #1
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp
	inc	sp
	inc	sp		; callee-cleanup: skip 4 bytes of stack args
	push	bc
	ret

;===------------------------------------------------------------------------===;
; ___gtsf2 / ___gesf2 - Float comparison (GT/GE variant)
;
; Same as ___cmpsf2 but returns -1 for NaN (so > 0 / >= 0 checks fail).
; GCC convention: __gtsf2/__gesf2 must return <= 0 for unordered (NaN).
;===------------------------------------------------------------------------===;
___gtsf2:
___gesf2:
	push	ix
	ld	ix, #0
	add	ix, sp

	; --- NaN check: a ---
	ld	a, h
	and	#0x7F
	cp	#0x7F
	jr	nz, __cmpgt_nan_a_no
	bit	7, l
	jr	z, __cmpgt_nan_a_no
	ld	a, l
	and	#0x7F
	or	d
	or	e
	jp	nz, __cmp_lt		; a is NaN → return -1
__cmpgt_nan_a_no:

	; --- NaN check: b ---
	ld	a, 7(ix)
	and	#0x7F
	cp	#0x7F
	jp	nz, __cmp_begin
	bit	7, 6(ix)
	jp	z, __cmp_begin
	ld	a, 6(ix)
	and	#0x7F
	or	5(ix)
	or	4(ix)
	jp	nz, __cmp_lt		; b is NaN → return -1
	jp	__cmp_begin		; not NaN (exp=255, mant=0 → Inf)

;===------------------------------------------------------------------------===;
; ___cmpsf2_fast - Fast-math float comparison (no NaN/±0 checks)
;
; Input:  HLDE = a, stack = b
; Output: DE = -1 (a<b), 0 (a==b), +1 (a>b)
;===------------------------------------------------------------------------===;
___cmpsf2_fast:
___eqsf2_fast:
___nesf2_fast:
___ltsf2_fast:
___lesf2_fast:
___gtsf2_fast:
___gesf2_fast:
	push	ix
	ld	ix, #0
	add	ix, sp
	jp	__cmp_begin

;===------------------------------------------------------------------------===;
; ___cmpsf2 / ___eqsf2 / ___nesf2 / ___ltsf2 / ___lesf2
; Three-way float comparison
;
; Input:  HLDE = a, stack = b
; Output: DE = -1 if a<b, 0 if a==b, +1 if a>b
;         If either is NaN: DE = +1 (unordered)
;
; __gtsf2/__gesf2 are separate entry points (return -1 for NaN).
;===------------------------------------------------------------------------===;
___cmpsf2:
___eqsf2:
___nesf2:
___ltsf2:
___lesf2:
	push	ix
	ld	ix, #0
	add	ix, sp

	; --- NaN check: a ---
	ld	a, h
	and	#0x7F
	cp	#0x7F
	jr	nz, __cmp_nan_a_no
	bit	7, l
	jr	z, __cmp_nan_a_no
	ld	a, l
	and	#0x7F
	or	d
	or	e
	jr	nz, __cmp_gt		; a is NaN → return +1
__cmp_nan_a_no:

	; --- NaN check: b ---
	ld	a, 7(ix)
	and	#0x7F
	cp	#0x7F
	jr	nz, __cmp_begin
	bit	7, 6(ix)
	jr	z, __cmp_begin
	ld	a, 6(ix)
	and	#0x7F
	or	5(ix)
	or	4(ix)
	jr	nz, __cmp_gt		; b is NaN → return +1
__cmp_begin:

	; --- ±0 == -0 check ---
	; If a is ±0, check if b is also ±0
	ld	a, h
	and	#0x7F
	or	l
	or	d
	or	e
	jr	nz, __cmp_signs
	; a is ±0
	ld	a, 7(ix)
	and	#0x7F
	or	6(ix)
	or	5(ix)
	or	4(ix)
	jr	z, __cmp_eq		; b is also ±0 → equal

__cmp_signs:
	; --- Compare signs ---
	ld	a, h
	xor	7(ix)		; bit 7 differs if different signs
	bit	7, a
	jr	z, __cmp_same_sign
	; Different signs: check who is negative
	bit	7, h
	jr	nz, __cmp_lt		; a negative, b positive → a < b
	jr	__cmp_gt		; a positive, b negative → a > b

__cmp_same_sign:
	; Same sign: compare magnitude (|H|, L, D, E) as big-endian unsigned
	; Consistently compute b_byte - a_byte; carry=1 → |b|<|a| (i.e. |a|>|b|)
	ld	a, h
	and	#0x7F
	ld	b, a		; B = |a_H|
	ld	a, 7(ix)
	and	#0x7F		; A = |b_H|
	sub	b		; b_H - a_H
	jr	nz, __cmp_mag_diff
	ld	a, 6(ix)
	sub	l		; b_L - a_L
	jr	nz, __cmp_mag_diff
	ld	a, 5(ix)
	sub	d		; b_D - a_D
	jr	nz, __cmp_mag_diff
	ld	a, 4(ix)
	sub	e		; b_E - a_E
	jr	nz, __cmp_mag_diff
	jr	__cmp_eq		; all bytes equal

__cmp_mag_diff:
	; After SUB: carry=1 → b_byte < a_byte → |a| > |b|
	;            carry=0 → b_byte > a_byte → |a| < |b|
	; If positive: |a|>|b| → a>b, |a|<|b| → a<b
	; If negative: |a|>|b| → a<b, |a|<|b| → a>b
	bit	7, h		; check sign (same for both)
	jr	z, __cmp_mag_pos
	; Both negative: flip meaning
	jr	c, __cmp_lt	; carry → |a|>|b| → a<b (negative)
	jr	__cmp_gt	; no carry → |a|<|b| → a>b (negative)
__cmp_mag_pos:
	jr	c, __cmp_gt	; carry → |a|>|b| → a>b (positive)
	jr	__cmp_lt	; no carry → |a|<|b| → a<b (positive)

__cmp_lt:
	ld	de, #-1		; 0xFFFF
	jr	__cmp_ret
__cmp_eq:
	ld	de, #0
	jr	__cmp_ret
__cmp_gt:
	ld	de, #1
__cmp_ret:
	pop	ix
	pop	hl		; return address
	inc	sp
	inc	sp
	inc	sp
	inc	sp		; callee-cleanup: skip 4 bytes of stack args
	jp	(hl)

;===------------------------------------------------------------------------===;
; ___fixsfsi - Convert float to signed int32
;
; Input:  HLDE = float
; Output: HLDE = signed int32
;
; Algorithm:
;   1. Extract sign, exponent, mantissa
;   2. If exp < 127 (|value| < 1.0) → return 0
;   3. If exp >= 127+31 → overflow (clamp to INT32_MAX/MIN)
