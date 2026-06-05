; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl ___floatsisf
	.globl __sm_flsi_nz
	.globl __sm_flsi_pos
	.globl __sm_flsi_norm
	.globl __sm_flsi_nlp
	.globl __sm_flsi_normed
	.globl __sm_flsi_rndup
	.globl __sm_flsi_nornd
	.globl __sm_flsi_e0
	.globl ___floatunsisf

___floatsisf:
	; Check for zero
	ld	a, d
	or	e
	or	b
	or	c
	jr	nz, __sm_flsi_nz
	ret			; DEBC already 0 = +0.0

__sm_flsi_nz:
	; Save sign
	ld	a, d
	and	#0x80
	push	af		; sign on stack

	; Take absolute value if negative
	bit	7, d
	jr	z, __sm_flsi_pos
	; Negate DEBC
	ld	a, c
	cpl
	ld	c, a
	ld	a, b
	cpl
	ld	b, a
	ld	a, e
	cpl
	ld	e, a
	ld	a, d
	cpl
	ld	d, a
	inc	c
	jr	nz, __sm_flsi_pos
	inc	b
	jr	nz, __sm_flsi_pos
	inc	e
	jr	nz, __sm_flsi_pos
	inc	d
__sm_flsi_pos:
	; D:E:B:C = |value| (positive 32-bit)
	; Normalize: shift left until D[7] is set.
	; H = exponent (starts at 158 = 127+31)
	ld	h, #158

	; Skip zero high bytes for speed
	ld	a, d
	or	a
	jr	nz, __sm_flsi_norm
	; D=0, shift left 8: D←E, E←B, B←C, C←0
	ld	d, e
	ld	e, b
	ld	b, c
	ld	c, #0
	ld	a, h
	sub	#8
	ld	h, a
	ld	a, d
	or	a
	jr	nz, __sm_flsi_norm
	; Still 0, shift another 8
	ld	d, e
	ld	e, b
	ld	b, #0
	ld	a, h
	sub	#8
	ld	h, a
	ld	a, d
	or	a
	jr	nz, __sm_flsi_norm
	; Another 8
	ld	d, e
	ld	e, #0
	ld	a, h
	sub	#8
	ld	h, a

__sm_flsi_norm:
	; D != 0. Shift left until D[7] is set.
__sm_flsi_nlp:
	bit	7, d
	jr	nz, __sm_flsi_normed
	sla	c
	rl	b
	rl	e
	rl	d
	dec	h		; exponent--
	jr	__sm_flsi_nlp

__sm_flsi_normed:
	; D[7] is set. Mantissa = D:E:B (24 bits), C = round info.
	; Round bit = C[7], Sticky = C[6:0], Guard = B[0]

	; Round-to-nearest-even
	bit	7, c		; R (round bit)
	jr	z, __sm_flsi_nornd
	; R=1: check sticky for tie-breaking
	ld	a, c
	and	#0x7F		; sticky bits
	jr	nz, __sm_flsi_rndup	; S!=0 → round up
	; S=0, R=1: tie. Round to even (check G=B[0])
	bit	0, b
	jr	z, __sm_flsi_nornd	; G=0 → round down (even)
__sm_flsi_rndup:
	; Add 1 to mantissa D:E:B
	inc	b
	jr	nz, __sm_flsi_nornd
	inc	e
	jr	nz, __sm_flsi_nornd
	inc	d
	jr	nz, __sm_flsi_nornd
	; Overflow: D was 0xFF → 0x00
	ld	d, #0x80
	inc	h		; exponent++
__sm_flsi_nornd:
	; Pack: sign(1) + exponent(8) + mantissa[22:0](23)
	; Mantissa = D:E:B with D[7] = implicit bit
	; Output DEBC:
	;   result_C = B                          (mantissa[7:0])
	;   result_B = E                          (mantissa[15:8])
	;   result_E = (D & 0x7F) | (H[0] << 7)  (exp[0]:mantissa[22:16])
	;   result_D = sign | (H >> 1)            (sign:exp[7:1])
	ld	c, b		; result_C = mantissa[7:0]
	ld	b, e		; result_B = mantissa[15:8]
	ld	a, d
	and	#0x7F		; remove implicit bit
	bit	0, h
	jr	z, __sm_flsi_e0
	or	#0x80		; set exp[0]
__sm_flsi_e0:
	ld	e, a		; result_E = exp_lo:mant_hi
	; result_D = sign | (exp >> 1)
	ld	a, h
	srl	a		; A = exp >> 1
	ld	l, a		; L = exp >> 1 (temp)
	pop	af		; A = sign (0x00 or 0x80)
	and	#0x80
	or	l		; A = sign | (exp >> 1)
	ld	d, a
	ret

;===------------------------------------------------------------------------===;
; ___floatunsisf - Convert unsigned int32 to float
;
; Input:  DEBC = unsigned int32
; Output: DEBC = float
;
; Same as ___floatsisf but no sign handling. Always positive.
;===------------------------------------------------------------------------===;
___floatunsisf:
	; Check for zero
	ld	a, d
	or	e
	or	b
	or	c
	ret	z		; return +0.0

	; Push sign = 0 (positive) for shared pack code
	xor	a
	push	af
	; Fall through to __sm_flsi_pos
	jr	__sm_flsi_pos
