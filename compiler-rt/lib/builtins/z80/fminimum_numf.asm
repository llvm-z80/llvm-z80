; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _fminimum_numf

;===------------------------------------------------------------------------===;
; _fminimum_numf - IEEE-754 2019 minimumNumber for floats (C23 fminimum_numf)
;
; Input:  HLDE = a, stack = b  (callee-cleanup for b)
; Output: HLDE = minimumNumber(a, b)
;         If one is NaN, return the other.
;         For equal values it honors signed-zero order (-0 < +0): if either
;         operand is -0, the result is -0.
;
; Differs from fminf (minNum) only in signed-zero handling.
;
; Uses ___cmpsf2 (callee-cleanup, returns DE: -1/0/+1).
;===------------------------------------------------------------------------===;
_fminimum_numf:
	push	ix
	ld	ix, #0
	add	ix, sp
	; IX+4..7: b (E2,D2,L2,H2)

	push	hl		; IX-2: a_L, IX-1: a_H
	push	de		; IX-4: a_E, IX-3: a_D

	; --- NaN check: a ---
	ld	a, h
	and	#0x7F
	cp	#0x7F
	jr	nz, __fminimumf_a_ok
	bit	7, l
	jr	z, __fminimumf_a_ok
	ld	a, l
	and	#0x7F
	or	d
	or	e
	jr	nz, __fminimumf_ret_b	; a is NaN → return b
__fminimumf_a_ok:

	; --- NaN check: b ---
	ld	a, 7(ix)
	and	#0x7F
	cp	#0x7F
	jr	nz, __fminimumf_cmp
	bit	7, 6(ix)
	jr	z, __fminimumf_cmp
	ld	a, 6(ix)
	and	#0x7F
	or	5(ix)
	or	4(ix)
	jr	nz, __fminimumf_ret_a	; b is NaN → return a

__fminimumf_cmp:
	ld	b, 7(ix)
	ld	c, 6(ix)
	push	bc		; H2:L2
	ld	b, 5(ix)
	ld	c, 4(ix)
	push	bc		; D2:E2
	ld	h, -1(ix)
	ld	l, -2(ix)
	ld	d, -3(ix)
	ld	e, -4(ix)
	call	___cmpsf2	; callee-cleanup removes 4 bytes, DE = result
	bit	7, d
	jr	nz, __fminimumf_ret_a	; a < b → return a
	ld	a, d
	or	e
	jr	nz, __fminimumf_ret_b	; a > b → return b

	; a == b: signed-zero order. result = a, sign = sign(a) | sign(b).
	; (For non-zero equals the sign bits match, so this is a no-op there.)
	ld	h, -1(ix)
	ld	l, -2(ix)
	ld	d, -3(ix)
	ld	e, -4(ix)
	bit	7, 7(ix)	; sign of b
	jr	z, __fminimumf_done
	set	7, h		; either operand is -0 → result is -0
	jr	__fminimumf_done

__fminimumf_ret_b:
	ld	h, 7(ix)
	ld	l, 6(ix)
	ld	d, 5(ix)
	ld	e, 4(ix)
	jr	__fminimumf_done

__fminimumf_ret_a:
	ld	h, -1(ix)
	ld	l, -2(ix)
	ld	d, -3(ix)
	ld	e, -4(ix)

__fminimumf_done:
	ld	sp, ix		; discard saved a
	pop	ix
	; Callee-cleanup: skip 4 bytes of b
	pop	bc		; return address
	inc	sp
	inc	sp
	inc	sp
	inc	sp
	push	bc
	ret
