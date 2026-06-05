; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___mulsi3

;===------------------------------------------------------------------------===;
; ___mulsi3 - 32-bit multiply (low 32 bits of result)
;
; Input:  HLDE = a (H:L = a_hi, D:E = a_lo)
;         stack: b (4-5(ix) = b_lo, 6-7(ix) = b_hi)
; Output: HLDE = a * b (low 32 bits)
;
; Algorithm: Product decomposition using 16-bit multiplies
;   a * b = a_lo*b_lo + (a_lo*b_hi + a_hi*b_lo) << 16
;   Uses __mulhi3 (low 16 bits) and __umulhi3 (high 16 bits)
;===------------------------------------------------------------------------===;
___mulsi3:
	push	ix
	ld	ix, #0
	add	ix, sp
	; Save a_hi (HL) and a_lo (DE)
	push	hl		; -2(ix),-1(ix) = a_hi  [L at -2, H at -1]
	push	de		; -4(ix),-3(ix) = a_lo  [E at -4, D at -3]
	push	af		; -6(ix),-5(ix) = temp (result_hi accumulator)
	push	af		; -8(ix),-7(ix) = temp (result_lo)

	; Step 1: umulhi3(a_lo, b_lo) → high 16 bits of a_lo * b_lo
	ex	de, hl		; HL = a_lo
	ld	e, 4(ix)
	ld	d, 5(ix)	; DE = b_lo
	call	___umulhi3	; DE = umulh(a_lo, b_lo)
	ld	-6(ix), e
	ld	-5(ix), d	; save to result_hi accumulator

	; Step 2: mulhi3(a_lo, b_lo) → low 16 bits of a_lo * b_lo
	ld	l, -4(ix)
	ld	h, -3(ix)	; HL = a_lo
	ld	e, 4(ix)
	ld	d, 5(ix)	; DE = b_lo
	call	___mulhi3	; DE = a_lo * b_lo (low 16)
	ld	-8(ix), e
	ld	-7(ix), d	; save to result_lo

	; Step 3: mulhi3(a_lo, b_hi) → cross product 1
	; Skip if b_hi == 0 (common when multiplying by small values)
	ld	a, 6(ix)
	or	7(ix)
	jr	z, ___mulsi3_skip_bhi
	ld	l, -4(ix)
	ld	h, -3(ix)	; HL = a_lo
	ld	e, 6(ix)
	ld	d, 7(ix)	; DE = b_hi
	call	___mulhi3	; DE = a_lo * b_hi (low 16)
	; Add to result_hi
	ld	l, -6(ix)
	ld	h, -5(ix)
	add	hl, de
	ld	-6(ix), l
	ld	-5(ix), h
___mulsi3_skip_bhi:

	; Step 4: mulhi3(a_hi, b_lo) → cross product 2
	; Skip if a_hi == 0
	ld	a, -2(ix)
	or	-1(ix)
	jr	z, ___mulsi3_skip_ahi
	ld	l, -2(ix)
	ld	h, -1(ix)	; HL = a_hi
	ld	e, 4(ix)
	ld	d, 5(ix)	; DE = b_lo
	call	___mulhi3	; DE = a_hi * b_lo (low 16)
	; Add to result_hi
	ld	l, -6(ix)
	ld	h, -5(ix)
	add	hl, de		; HL = final result_hi
	jr	___mulsi3_done

___mulsi3_skip_ahi:
	ld	l, -6(ix)
	ld	h, -5(ix)	; HL = result_hi (no cross product 2)

___mulsi3_done:

	; Assemble result: HLDE
	ld	e, -8(ix)
	ld	d, -7(ix)	; DE = result_lo
	; HL = result_hi

	ld	sp, ix
	pop	ix
	ret
