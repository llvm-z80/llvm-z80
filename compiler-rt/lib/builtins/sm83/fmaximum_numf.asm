; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _fmaximum_numf

;===------------------------------------------------------------------------===;
; _fmaximum_numf - IEEE-754 2019 maximumNumber for floats (SM83)
;
; Input:  DEBC = a, stack = b  (callee-cleanup for b)
; Output: DEBC = maximumNumber(a, b)
;         If one is NaN, return the other. For equal values honors signed-zero
;         order (-0 < +0): the result is +0 unless both operands are -0.
;
; Differs from fmaxf (maxNum) only in signed-zero handling.
;
; SM83 stack frame (no IX): saved_a(4) + ret_addr(2) + b(4).
;===------------------------------------------------------------------------===;
_fmaximum_numf:
	push	de		; a high
	push	bc		; a low
	; SP+0..1: a_C,a_B  SP+2..3: a_E,a_D  SP+4..5: ret  SP+6..9: b_C,b_B,b_E,b_D

	; NaN check a
	ld	a, d
	and	#0x7F
	cp	#0x7F
	jr	nz, __fmaximumf_a_ok
	bit	7, e
	jr	z, __fmaximumf_a_ok
	ld	a, e
	and	#0x7F
	or	b
	or	c
	jr	nz, __fmaximumf_ret_b
__fmaximumf_a_ok:

	; NaN check b
	ld	hl, #9
	add	hl, sp
	ld	a, (hl)		; b_D
	and	#0x7F
	cp	#0x7F
	jr	nz, __fmaximumf_cmp
	dec	hl		; b_E
	bit	7, (hl)
	jr	z, __fmaximumf_cmp
	ld	a, (hl)
	and	#0x7F
	dec	hl		; b_B
	or	(hl)
	dec	hl		; b_C
	or	(hl)
	jr	nz, __fmaximumf_ret_a

__fmaximumf_cmp:
	; Push b copy for cmpsf2
	ld	hl, #8
	add	hl, sp		; → b_E
	ld	a, (hl)
	inc	hl		; b_D
	ld	h, (hl)
	ld	l, a		; HL = b_D:b_E
	push	hl		; D2:E2
	ld	hl, #8
	add	hl, sp		; → b_C (stack grew by 2)
	ld	a, (hl)
	inc	hl		; b_B
	ld	h, (hl)
	ld	l, a		; HL = b_B:b_C
	push	hl		; B2:C2

	; Reload a into DEBC
	ld	hl, #4
	add	hl, sp		; → a_C (stack grew by 4)
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	call	___cmpsf2	; callee-cleanup removes 4, BC = result
	bit	7, b
	jr	nz, __fmaximumf_ret_b	; a < b → b
	ld	a, b
	or	c
	jr	nz, __fmaximumf_ret_a	; a > b → a

	; a == b: result = a, sign = sign(a) & sign(b).
	ld	hl, #0
	add	hl, sp
	ld	c, (hl)		; a_C
	inc	hl
	ld	b, (hl)		; a_B
	inc	hl
	ld	e, (hl)		; a_E
	inc	hl
	ld	d, (hl)		; a_D
	ld	hl, #9
	add	hl, sp
	bit	7, (hl)		; sign of b
	jr	nz, __fmaximumf_done
	res	7, d		; b is +0 → result +0
	jr	__fmaximumf_done

__fmaximumf_ret_b:
	ld	hl, #6
	add	hl, sp
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	jr	__fmaximumf_done

__fmaximumf_ret_a:
	pop	bc		; a low
	pop	de		; a high
	pop	hl		; ret addr
	inc	sp
	inc	sp
	inc	sp
	inc	sp
	jp	(hl)

__fmaximumf_done:
	pop	hl		; discard a low
	pop	hl		; discard a high
	pop	hl		; ret addr
	inc	sp
	inc	sp
	inc	sp
	inc	sp
	jp	(hl)
