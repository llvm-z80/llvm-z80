; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___muldi3

;===------------------------------------------------------------------------===;
; ___muldi3 - 64-bit multiply (low 64 bits of result) for SM83
;
; Calling convention (sret demotion, SDCC __sdcccall(1)):
;   Stack: return address (2B), sret pointer (2B), a (8B), b (8B)
;
; Stack layout at entry:
;   SP+0,1   = return address
;   SP+2,3   = sret pointer
;   SP+4,5   = a[0] (word 0, lowest)
;   SP+6,7   = a[1]
;   SP+8,9   = a[2]
;   SP+10,11 = a[3] (word 3, highest)
;   SP+12,13 = b[0]
;   SP+14,15 = b[1]
;   SP+16,17 = b[2]
;   SP+18,19 = b[3]
;
; After push af + allocate 8:
;   SP+0..7   = result[0..3] (local, 8 bytes)
;   SP+8,9    = push af (dummy)
;   SP+10,11  = return address
;   SP+12,13  = sret pointer
;   SP+14,15  = a[0]
;   SP+16,17  = a[1]
;   SP+18,19  = a[2]
;   SP+20,21  = a[3]
;   SP+22,23  = b[0]
;   SP+24,25  = b[1]
;   SP+26,27  = b[2]
;   SP+28,29  = b[3]
;
; Inside subroutine calls (call adds +2), all offsets shift by +2.
;
; SM83 __mulhi3: DE=arg1, BC=arg2, return BC
; SM83 __umulhi3: DE=arg1, BC=arg2, return BC
;===------------------------------------------------------------------------===;

; Helper macro-like pattern: load a[i] into DE, b[j] into BC
; Since we can't use macros easily in sdasz80, we inline each call.

___muldi3:
	push	af		; dummy (8,9)
	add	sp, #-8		; allocate result (0..7)

	; Zero result
	ldhl	sp, #0
	xor	a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl), a

	;=== a[0] * b[0] → result[0], hi → result[1] ===
	ldhl	sp, #14
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)	; DE = a[0]
	ldhl	sp, #22
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)	; BC = b[0]
	call	___mulhi3	; BC = lo(a0*b0)
	ldhl	sp, #0
	ld	(hl), c
	inc	hl
	ld	(hl), b		; result[0] = lo(a0*b0)

	ldhl	sp, #14
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp, #22
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	call	___umulhi3	; BC = hi(a0*b0)
	ldhl	sp, #2
	ld	(hl), c
	inc	hl
	ld	(hl), b		; result[1] = hi(a0*b0)

	;=== a[0] * b[1] → add to result[1], carry to result[2..3] ===
	ldhl	sp, #14
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp, #24
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	call	___mulhi3
	; Add BC to result[1]
	ldhl	sp, #2
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a
	jr	nc, _sm_a0b1l_nc
	; Carry into result[2..3] (byte-wise inc)
	ldhl	sp, #4
	inc	(hl)
	jr	nz, _sm_a0b1l_nc
	inc	hl
	inc	(hl)
	jr	nz, _sm_a0b1l_nc
	inc	hl
	inc	(hl)
	jr	nz, _sm_a0b1l_nc
	inc	hl
	inc	(hl)
_sm_a0b1l_nc:

	ldhl	sp, #14
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp, #24
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	call	___umulhi3
	ldhl	sp, #4
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a
	jr	nc, _sm_a0b1h_nc
	ldhl	sp, #6
	inc	(hl)
	jr	nz, _sm_a0b1h_nc
	inc	hl
	inc	(hl)
_sm_a0b1h_nc:

	;=== a[1] * b[0] → add to result[1], carry to result[2..3] ===
	ldhl	sp, #16
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp, #22
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	call	___mulhi3
	ldhl	sp, #2
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a
	jr	nc, _sm_a1b0l_nc
	ldhl	sp, #4
	inc	(hl)
	jr	nz, _sm_a1b0l_nc
	inc	hl
	inc	(hl)
	jr	nz, _sm_a1b0l_nc
	inc	hl
	inc	(hl)
	jr	nz, _sm_a1b0l_nc
	inc	hl
	inc	(hl)
_sm_a1b0l_nc:

	ldhl	sp, #16
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp, #22
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	call	___umulhi3
	ldhl	sp, #4
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a
	jr	nc, _sm_a1b0h_nc
	ldhl	sp, #6
	inc	(hl)
	jr	nz, _sm_a1b0h_nc
	inc	hl
	inc	(hl)
_sm_a1b0h_nc:

	;=== a[0] * b[2] → add to result[2], carry to result[3] ===
	ldhl	sp, #14
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp, #26
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	call	___mulhi3
	ldhl	sp, #4
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a
	jr	nc, _sm_a0b2l_nc
	ldhl	sp, #6
	inc	(hl)
	jr	nz, _sm_a0b2l_nc
	inc	hl
	inc	(hl)
_sm_a0b2l_nc:

	ldhl	sp, #14
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp, #26
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	call	___umulhi3
	ldhl	sp, #6
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a

	;=== a[1] * b[1] → add to result[2], carry to result[3] ===
	ldhl	sp, #16
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp, #24
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	call	___mulhi3
	ldhl	sp, #4
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a
	jr	nc, _sm_a1b1l_nc
	ldhl	sp, #6
	inc	(hl)
	jr	nz, _sm_a1b1l_nc
	inc	hl
	inc	(hl)
_sm_a1b1l_nc:

	ldhl	sp, #16
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp, #24
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	call	___umulhi3
	ldhl	sp, #6
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a

	;=== a[2] * b[0] → add to result[2], carry to result[3] ===
	ldhl	sp, #18
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp, #22
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	call	___mulhi3
	ldhl	sp, #4
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a
	jr	nc, _sm_a2b0l_nc
	ldhl	sp, #6
	inc	(hl)
	jr	nz, _sm_a2b0l_nc
	inc	hl
	inc	(hl)
_sm_a2b0l_nc:

	ldhl	sp, #18
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp, #22
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	call	___umulhi3
	ldhl	sp, #6
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a

	;=== result[3] only: a0*b3 + a1*b2 + a2*b1 + a3*b0 ===
	ldhl	sp, #14
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp, #28
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	call	___mulhi3
	ldhl	sp, #6
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a

	ldhl	sp, #16
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp, #26
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	call	___mulhi3
	ldhl	sp, #6
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a

	ldhl	sp, #18
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp, #24
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	call	___mulhi3
	ldhl	sp, #6
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a

	ldhl	sp, #20
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp, #22
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	call	___mulhi3
	ldhl	sp, #6
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a

	;=== Copy result to sret pointer ===
	; Load sret pointer
	ldhl	sp, #12
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)		; DE = sret pointer
	; Copy result[0..3] → sret
	ldhl	sp, #0
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl)
	ld	(de), a

	; Cleanup
	add	sp, #10		; free result(8) + dummy(2)
	pop	hl		; return address
	add	sp, #18		; callee-cleanup: skip sret(2) + args(16)
	jp	(hl)
