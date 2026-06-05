; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _fminf

;===------------------------------------------------------------------------===;
; _fminf - Return minimum of two floats (SM83)
;
; Input:  DEBC = a, stack = b  (callee-cleanup for b)
; Output: DEBC = fmin(a, b)
;
; SM83 stack frame (no IX): use SP-relative access via HL.
; After saving regs: saved_a(4) + ret_addr(2) + b(4)
;===------------------------------------------------------------------------===;
_fminf:
	; Save a on stack
	push	de		; a high
	push	bc		; a low
	; SP: [a_BC][a_DE][ret_addr][b]
	; SP+0..1: a_C,a_B  SP+2..3: a_E,a_D
	; SP+4..5: ret_addr
	; SP+6: b_C, SP+7: b_B, SP+8: b_E, SP+9: b_D

	; NaN check a: D[6:0]=0x7F && E[7]=1 && (E[6:0]|B|C)!=0
	ld	a, d
	and	#0x7F
	cp	#0x7F
	jr	nz, __fminf_a_ok
	bit	7, e
	jr	z, __fminf_a_ok
	ld	a, e
	and	#0x7F
	or	b
	or	c
	jr	nz, __fminf_ret_b
__fminf_a_ok:

	; NaN check b
	ld	hl, #9
	add	hl, sp
	ld	a, (hl)		; b_D
	and	#0x7F
	cp	#0x7F
	jr	nz, __fminf_cmp
	dec	hl		; b_E
	bit	7, (hl)
	jr	z, __fminf_cmp
	ld	a, (hl)
	and	#0x7F
	dec	hl		; b_B
	or	(hl)
	dec	hl		; b_C
	or	(hl)
	jr	nz, __fminf_ret_a

__fminf_cmp:
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
	; BC: -1/0/+1
	bit	7, b
	jr	nz, __fminf_ret_a	; a < b

__fminf_ret_b:
	ld	hl, #6
	add	hl, sp
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	jr	__fminf_done

__fminf_ret_a:
	pop	bc		; a low
	pop	de		; a high
	; Clean up b from original stack
	pop	hl		; ret addr
	inc	sp
	inc	sp
	inc	sp
	inc	sp
	jp	(hl)

__fminf_done:
	; Discard saved a
	pop	hl		; a low (discard)
	pop	hl		; a high (discard)
	; Callee-cleanup b
	pop	hl		; ret addr
	inc	sp
	inc	sp
	inc	sp
	inc	sp
	jp	(hl)
