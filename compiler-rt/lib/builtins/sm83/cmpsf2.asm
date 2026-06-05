; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___unordsf2
	.globl __sm_unord_ck_b
	.globl __sm_unord_no
	.globl __sm_unord_yes
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
	.globl __sm_cmp_nan_a_no
	.globl __sm_cmp_begin
	.globl __sm_cmp_signs
	.globl __sm_cmp_same_sign
	.globl __sm_cmp_a_gt_s
	.globl __sm_cmp_a_lt_s
	.globl __sm_cmp_lt_p
	.globl __sm_cmp_eq_p
	.globl __sm_cmp_gt_p
	.globl ___gtsf2
	.globl ___gesf2
	.globl __sm_cmpgt_nan_a_no
	.globl __sm_cmpgt_nan_lt


___unordsf2:
	; Check a: exponent==255 && mantissa!=0
	; NaN: D[6:0]==0x7F && E[7]==1 && (E[6:0]|B|C)!=0
	ld	a, d
	and	#0x7F
	cp	#0x7F
	jr	nz, __sm_unord_ck_b
	bit	7, e
	jr	z, __sm_unord_ck_b
	ld	a, e
	and	#0x7F
	or	b
	or	c
	jr	nz, __sm_unord_yes
__sm_unord_ck_b:
	; Check b: stack at SP+2..SP+5
	ldhl	sp, #5		; HL = &b_D
	ld	a, (hl)
	and	#0x7F
	cp	#0x7F
	jr	nz, __sm_unord_no
	ldhl	sp, #4		; HL = &b_E
	ld	a, (hl)
	bit	7, a
	jr	z, __sm_unord_no
	and	#0x7F
	jr	nz, __sm_unord_yes
	ldhl	sp, #3		; HL = &b_B
	ld	a, (hl)
	or	a
	jr	nz, __sm_unord_yes
	ldhl	sp, #2		; HL = &b_C
	ld	a, (hl)
	or	a
	jr	nz, __sm_unord_yes
__sm_unord_no:
	ld	bc, #0
	pop	hl		; return address
	add	sp, #4		; callee-cleanup: skip 4 bytes of stack args
	jp	(hl)
__sm_unord_yes:
	ld	bc, #1
	pop	hl		; return address
	add	sp, #4		; callee-cleanup: skip 4 bytes of stack args
	jp	(hl)

;===------------------------------------------------------------------------===;
; ___cmpsf2_fast - Fast-math float comparison (no NaN/±0 checks)
;===------------------------------------------------------------------------===;
___cmpsf2_fast:
___eqsf2_fast:
___nesf2_fast:
___ltsf2_fast:
___lesf2_fast:
___gtsf2_fast:
___gesf2_fast:
	push	de
	jp	__sm_cmp_begin

;===------------------------------------------------------------------------===;
; ___cmpsf2 / ___eqsf2 / ___nesf2 / ___ltsf2 / ___lesf2
; Three-way float comparison
;
; Input:  DEBC = a, stack = b
; Output: BC = -1 if a<b, 0 if a==b, +1 if a>b
;         If either is NaN: BC = +1 (unordered)
;===------------------------------------------------------------------------===;

___cmpsf2:
___eqsf2:
___nesf2:
___ltsf2:
___lesf2:
	push	de		; save D for sign check; SP -= 2
	; Stack: SP+0=E, SP+1=D, SP+2=ret_lo, SP+3=ret_hi
	;        SP+4=b_C, SP+5=b_B, SP+6=b_E, SP+7=b_D

	; --- NaN check: a ---
	ld	a, d
	and	#0x7F
	cp	#0x7F
	jr	nz, __sm_cmp_nan_a_no
	bit	7, e
	jr	z, __sm_cmp_nan_a_no
	ld	a, e
	and	#0x7F
	or	b
	or	c
	jp	nz, __sm_cmp_gt_p	; a is NaN → return +1
__sm_cmp_nan_a_no:

	; --- NaN check: b ---
	ldhl	sp, #7		; HL = &b_D
	ld	a, (hl)
	and	#0x7F
	cp	#0x7F
	jr	nz, __sm_cmp_begin
	ldhl	sp, #6		; HL = &b_E
	ld	a, (hl)
	bit	7, a
	jr	z, __sm_cmp_begin
	and	#0x7F
	jp	nz, __sm_cmp_gt_p	; b is NaN → return +1
	ldhl	sp, #5		; HL = &b_B
	ld	a, (hl)
	or	a
	jp	nz, __sm_cmp_gt_p
	ldhl	sp, #4		; HL = &b_C
	ld	a, (hl)
	or	a
	jp	nz, __sm_cmp_gt_p

__sm_cmp_begin:
	; --- ±0 == -0 check ---
	ld	a, d
	and	#0x7F
	or	e
	or	b
	or	c
	jr	nz, __sm_cmp_signs
	; a is ±0, check if b is also ±0
	ldhl	sp, #7		; HL = &b_D
	ld	a, (hl)
	and	#0x7F
	ldhl	sp, #6		; HL = &b_E
	or	(hl)
	ldhl	sp, #5		; HL = &b_B
	or	(hl)
	ldhl	sp, #4		; HL = &b_C
	or	(hl)
	jr	z, __sm_cmp_eq_p	; b is also ±0 → equal

__sm_cmp_signs:
	; --- Compare signs ---
	ldhl	sp, #7		; HL = &b_D
	ld	a, d
	xor	(hl)		; bit 7 differs if different signs
	bit	7, a
	jr	z, __sm_cmp_same_sign
	; Different signs
	bit	7, d
	jr	nz, __sm_cmp_lt_p	; a negative, b positive → a < b
	jr	__sm_cmp_gt_p		; a positive, b negative → a > b

__sm_cmp_same_sign:
	; --- Save sign for magnitude comparison ---
	ld	a, d
	rlca			; rotate sign bit into carry, then bit 0
	and	#1		; A = 1 if negative, 0 if positive
	push	af		; save sign; SP -= 2
	; Stack offsets now: +2=E_sav, +3=D_sav, +4=ret_lo, +5=ret_hi
	;                    +6=b_C, +7=b_B, +8=b_E, +9=b_D

	; --- Magnitude comparison (byte by byte, big-endian) ---
	; Byte 3 (highest): |a_D| vs |b_D|
	ld	a, d
	and	#0x7F
	ld	d, a		; D = |a_D| (original D saved on stack)
	ldhl	sp, #9		; HL = &b_D
	ld	a, (hl)
	and	#0x7F		; A = |b_D|
	sub	d		; |b_D| - |a_D|
	jr	c, __sm_cmp_a_gt_s
	jr	nz, __sm_cmp_a_lt_s

	; Byte 2: a_E vs b_E
	ldhl	sp, #8		; HL = &b_E
	ld	a, (hl)
	sub	e		; b_E - a_E
	jr	c, __sm_cmp_a_gt_s
	jr	nz, __sm_cmp_a_lt_s

	; Byte 1: a_B vs b_B
	ldhl	sp, #7		; HL = &b_B
	ld	a, (hl)
	sub	b		; b_B - a_B
	jr	c, __sm_cmp_a_gt_s
	jr	nz, __sm_cmp_a_lt_s

	; Byte 0 (lowest): a_C vs b_C
	ldhl	sp, #6		; HL = &b_C
	ld	a, (hl)
	sub	c		; b_C - a_C
	jr	c, __sm_cmp_a_gt_s
	jr	nz, __sm_cmp_a_lt_s

	; All bytes equal
	pop	af		; discard sign
	jr	__sm_cmp_eq_p

__sm_cmp_a_gt_s:
	; |a| > |b|. If positive → a > b. If negative → a < b.
	pop	af		; A = sign (1=neg, 0=pos)
	or	a
	jr	nz, __sm_cmp_lt_p
	jr	__sm_cmp_gt_p

__sm_cmp_a_lt_s:
	; |a| < |b|. If positive → a < b. If negative → a > b.
	pop	af
	or	a
	jr	nz, __sm_cmp_gt_p
	jr	__sm_cmp_lt_p

__sm_cmp_lt_p:
	pop	de		; restore saved DE
	ld	bc, #-1		; 0xFFFF
	pop	hl		; return address
	add	sp, #4		; callee-cleanup: skip 4 bytes of stack args
	jp	(hl)
__sm_cmp_eq_p:
	pop	de
	ld	bc, #0
	pop	hl
	add	sp, #4
	jp	(hl)
__sm_cmp_gt_p:
	pop	de
	ld	bc, #1
	pop	hl
	add	sp, #4
	jp	(hl)

;===------------------------------------------------------------------------===;
; ___gtsf2 / ___gesf2 - Float comparison (GT/GE variant)
;
; Same as ___cmpsf2 but returns -1 for NaN (so > 0 / >= 0 checks fail).
; GCC convention: __gtsf2/__gesf2 must return <= 0 for unordered (NaN).
;
; Input:  DEBC = a, stack = b
; Output: BC = -1 if a<b or NaN, 0 if a==b, +1 if a>b
;===------------------------------------------------------------------------===;

___gtsf2:
___gesf2:
	push	de		; save D for sign check; SP -= 2
	; Stack: SP+0=E, SP+1=D, SP+2=ret_lo, SP+3=ret_hi
	;        SP+4=b_C, SP+5=b_B, SP+6=b_E, SP+7=b_D

	; --- NaN check: a ---
	ld	a, d
	and	#0x7F
	cp	#0x7F
	jr	nz, __sm_cmpgt_nan_a_no
	bit	7, e
	jr	z, __sm_cmpgt_nan_a_no
	ld	a, e
	and	#0x7F
	or	b
	or	c
	jr	nz, __sm_cmpgt_nan_lt	; a is NaN → return -1
__sm_cmpgt_nan_a_no:

	; --- NaN check: b ---
	ldhl	sp, #7		; HL = &b_D
	ld	a, (hl)
	and	#0x7F
	cp	#0x7F
	jp	nz, __sm_cmp_begin
	ldhl	sp, #6		; HL = &b_E
	ld	a, (hl)
	bit	7, a
	jp	z, __sm_cmp_begin
	and	#0x7F
	jr	nz, __sm_cmpgt_nan_lt	; b is NaN → return -1
	ldhl	sp, #5		; HL = &b_B
	ld	a, (hl)
	or	a
	jr	nz, __sm_cmpgt_nan_lt
	ldhl	sp, #4		; HL = &b_C
	ld	a, (hl)
	or	a
	jr	nz, __sm_cmpgt_nan_lt
	jp	__sm_cmp_begin		; not NaN (exp=255, mant=0 → Inf)

__sm_cmpgt_nan_lt:
	pop	de
	ld	bc, #-1
	pop	hl		; return address
	add	sp, #4		; callee-cleanup: skip 4 bytes of stack args
	jp	(hl)

;===------------------------------------------------------------------------===;
; ___subsf3_fast - Fast-math float subtraction (no NaN checks)
