; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___mulsi3

;===------------------------------------------------------------------------===;
; ___mulsi3 - 32-bit multiply (low 32 bits of result)
;
; Input:  DEBC = a (D:E = a_hi, B:C = a_lo)
;         stack+2..+5 = b (b_lo at +2,+3; b_hi at +4,+5)
; Output: DEBC = a * b (low 32 bits)
;
; Algorithm: Product decomposition using 16-bit multiplies
;   a * b = a_lo*b_lo + (a_lo*b_hi + a_hi*b_lo) << 16
;
; SM83 callee-cleanup: pops return addr, skips 4 bytes of stack args, jp (hl).
; __mulhi3: DE=arg1, BC=arg2, return BC
; __umulhi3: DE=arg1, BC=arg2, return BC
;
; Stack frame (after setup):
;   SP+0  = a_lo (2B, pushed BC)
;   SP+2  = a_hi (2B, pushed DE)
;   SP+4  = result_hi (2B, placeholder)
;   SP+6  = result_lo (2B, placeholder)
;   SP+8  = return addr (2B)
;   SP+10 = b_lo (2B, from caller)
;   SP+12 = b_hi (2B, from caller)
;===------------------------------------------------------------------------===;
___mulsi3:
	push	af		; SP+6 = result_lo placeholder
	push	af		; SP+4 = result_hi placeholder
	push	de		; SP+2 = a_hi
	push	bc		; SP+0 = a_lo

	; Step 1: umulhi3(a_lo, b_lo) -> high 16 bits of a_lo * b_lo
	ld	d, b
	ld	e, c		; DE = a_lo
	ldhl	sp, #10
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)		; BC = b_lo
	call	___umulhi3	; BC = umulh(a_lo, b_lo)
	; Save to result_hi
	ldhl	sp, #4
	ld	(hl), c
	inc	hl
	ld	(hl), b

	; Step 2: mulhi3(a_lo, b_lo) -> low 16 bits of a_lo * b_lo
	ldhl	sp, #0
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)		; DE = a_lo
	ldhl	sp, #10
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)		; BC = b_lo
	call	___mulhi3	; BC = a_lo * b_lo (low 16)
	; Save to result_lo
	ldhl	sp, #6
	ld	(hl), c
	inc	hl
	ld	(hl), b

	; Step 3: mulhi3(a_lo, b_hi) -> cross product 1
	; Skip if b_hi == 0 (common when multiplying by small values)
	ldhl	sp, #12
	ld	a, (hl+)
	or	(hl)
	jr	z, ___mulsi3_skip_bhi
	ldhl	sp, #0
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)		; DE = a_lo
	ldhl	sp, #12
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)		; BC = b_hi
	call	___mulhi3	; BC = a_lo * b_hi (low 16)
	; Add to result_hi
	ldhl	sp, #4
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a
___mulsi3_skip_bhi:

	; Step 4: mulhi3(a_hi, b_lo) -> cross product 2
	; Skip if a_hi == 0
	ldhl	sp, #2
	ld	a, (hl+)
	or	(hl)
	jr	z, ___mulsi3_skip_ahi
	ldhl	sp, #2
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)		; DE = a_hi
	ldhl	sp, #10
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)		; BC = b_lo
	call	___mulhi3	; BC = a_hi * b_lo (low 16)
	; Add to result_hi
	ldhl	sp, #4
	ld	a, (hl)
	add	a, c
	ld	(hl+), a
	ld	a, (hl)
	adc	a, b
	ld	(hl), a
___mulsi3_skip_ahi:

	; Assemble result: DEBC
	ldhl	sp, #4
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)		; DE = result_hi
	ldhl	sp, #6
	ld	a, (hl+)
	ld	c, a
	; B = (hl) but we need result_lo high byte
	ld	b, (hl)		; BC = result_lo

	; Clean up frame (4 pushes = 8 bytes)
	add	sp, #8

	; Callee-cleanup: pop return addr, skip 4 bytes of stack args
	pop	hl		; return address
	add	sp, #4		; skip b (4 bytes of stack args)
	jp	(hl)
