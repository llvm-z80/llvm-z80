; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl __neg32_debc
	.globl __udiv32_loop
	.globl __udiv32_dosub
	.globl __udiv32_skip
	.globl ___udivsi3
	.globl ___umodsi3
	.globl __neg32_on_stack
	.globl ___divsi3
	.globl ___divsi3_div_pos
	.globl ___modsi3
	.globl ___modsi3_div_pos

;===------------------------------------------------------------------------===;
__neg32_debc:
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
	ret	nz
	inc	b
	ret	nz
	inc	e
	ret	nz
	inc	d
	ret

;===------------------------------------------------------------------------===;
; __udiv32_loop - Core 32-bit unsigned division (restoring)
;
; Stack layout at entry (after call to this function):
;   SP+0  = return addr (2)
;   SP+2  = counter (2, pushed AF: A=32)
;   SP+4  = quotient byte 0 (lo)  \
;   SP+5  = quotient byte 1       | 4 bytes
;   SP+6  = quotient byte 2       |
;   SP+7  = quotient byte 3 (hi)  /
;   SP+8  = sign/dummy (2, pushed AF)
;   SP+10 = caller return addr (2)
;   SP+12 = divisor byte 0 (lo)  \
;   SP+13 = divisor byte 1       | 4 bytes on original caller's stack
;   SP+14 = divisor byte 2       |
;   SP+15 = divisor byte 3 (hi)  /
;
; Registers: DEBC = remainder (starts at 0)
; Quotient on stack (starts as dividend, shifted left each iteration)
;
; Per iteration:
;   1. Shift quotient left, MSB → carry
;   2. Shift carry into remainder (DEBC)
;   3. Compare remainder with divisor
;   4. If remainder >= divisor: subtract, set quotient bit 0
;===------------------------------------------------------------------------===;
__udiv32_loop:
	; --- Shift quotient left (4 bytes on stack) ---
	; After call, SP+0=retaddr. counter at SP+2. quotient at SP+4.
	ldhl	sp, #4
	ld	a, (hl)		; quotient byte 0
	add	a, a		; shift left, MSB → carry
	ld	(hl+), a
	ld	a, (hl)		; quotient byte 1
	rla			; shift left through carry
	ld	(hl+), a
	ld	a, (hl)		; quotient byte 2
	rla
	ld	(hl+), a
	ld	a, (hl)		; quotient byte 3
	rla
	ld	(hl), a		; store back (no hl+ needed)

	; --- Shift carry into remainder ---
	rl	c
	rl	b
	rl	e
	rl	d
	; If carry out from rl d → remainder > 32 bits → always >= divisor
	jr	c, __udiv32_dosub

	; --- Compare remainder (DEBC) with divisor (on stack at SP+12) ---
	ldhl	sp, #12
	ld	a, c
	sub	(hl)		; byte 0
	inc	hl
	ld	a, b
	sbc	a, (hl)		; byte 1
	inc	hl
	ld	a, e
	sbc	a, (hl)		; byte 2
	inc	hl
	ld	a, d
	sbc	a, (hl)		; byte 3
	jr	c, __udiv32_skip	; remainder < divisor

__udiv32_dosub:
	; --- Subtract divisor from remainder ---
	ldhl	sp, #12
	ld	a, c
	sub	(hl)
	ld	c, a
	inc	hl
	ld	a, b
	sbc	a, (hl)
	ld	b, a
	inc	hl
	ld	a, e
	sbc	a, (hl)
	ld	e, a
	inc	hl
	ld	a, d
	sbc	a, (hl)
	ld	d, a

	; --- Set quotient bit 0 ---
	ldhl	sp, #4
	inc	(hl)		; quotient byte 0 |= 1 (was even from shift)

__udiv32_skip:
	; --- Decrement counter and loop ---
	; Counter was pushed as AF: F at SP+2, A at SP+3
	ldhl	sp, #3
	ld	a, (hl)		; load counter (A byte of push AF)
	dec	a
	ld	(hl), a		; store back
	jr	nz, __udiv32_loop
	ret

;===------------------------------------------------------------------------===;
; ___udivsi3 - Unsigned 32-bit division
;
; Input:  DEBC = dividend, stack+2..+5 = divisor (after return addr)
; Output: DEBC = quotient
;
; Stack setup:
;   push dummy AF (for uniform layout with signed versions)
;   push dividend as initial quotient (4 bytes = 2 pushes)
;   push counter
;   call __udiv32_loop
;   pop quotient into DEBC
;===------------------------------------------------------------------------===;
___udivsi3:
	push	af		; dummy (sign slot, uniform with signed)
	push	de		; quotient hi = dividend hi
	push	bc		; quotient lo = dividend lo
	; DEBC = 0 (remainder)
	ld	d, #0
	ld	e, d
	ld	b, d
	ld	c, d
	; Counter
	ld	a, #32
	push	af		; counter on stack
	call	__udiv32_loop
	pop	af		; discard counter
	pop	bc		; quotient lo
	pop	de		; quotient hi
	pop	af		; discard dummy
	pop	hl		; return address
	add	sp, #4		; callee-cleanup: skip 4 bytes of stack args
	jp	(hl)

;===------------------------------------------------------------------------===;
; ___umodsi3 - Unsigned 32-bit modulo
;
; Input:  DEBC = dividend, stack+2..+5 = divisor
; Output: DEBC = remainder
;===------------------------------------------------------------------------===;
___umodsi3:
	push	af		; dummy
	push	de		; quotient hi = dividend hi
	push	bc		; quotient lo = dividend lo
	ld	d, #0
	ld	e, d
	ld	b, d
	ld	c, d
	ld	a, #32
	push	af
	call	__udiv32_loop
	pop	af		; discard counter
	add	sp, #4		; discard quotient (don't need it)
	pop	af		; discard dummy
	; DEBC already has remainder
	pop	hl		; return address
	add	sp, #4		; callee-cleanup: skip 4 bytes of stack args
	jp	(hl)

;===------------------------------------------------------------------------===;
; __neg32_on_stack - Negate 4-byte value on stack via HL pointer
;
; Input:  HL = pointer to byte 0 of 4-byte value on stack
; Output: value at (HL)..(HL+3) negated in place
; Clobbers: A, HL
;===------------------------------------------------------------------------===;
__neg32_on_stack:
	; CPL all 4 bytes
	ld	a, (hl)
	cpl
	ld	(hl+), a
	ld	a, (hl)
	cpl
	ld	(hl+), a
	ld	a, (hl)
	cpl
	ld	(hl+), a
	ld	a, (hl)
	cpl
	ld	(hl), a
	; Increment (add 1 to the 4-byte value)
	dec	hl
	dec	hl
	dec	hl		; HL back to byte 0
	inc	(hl)
	ld	a, (hl)
	or	a
	ret	nz		; no carry to propagate
	inc	hl
	inc	(hl)
	ld	a, (hl)
	or	a
	ret	nz
	inc	hl
	inc	(hl)
	ld	a, (hl)
	or	a
	ret	nz
	inc	hl
	inc	(hl)
	ret

;===------------------------------------------------------------------------===;
; ___divsi3 - Signed 32-bit division
;
; Input:  DEBC = dividend, stack+2..+5 = divisor
; Output: DEBC = quotient (sign = XOR of input signs)
;
; Approach: determine result sign, make both positive, unsigned divide,
;           apply sign to quotient.
;===------------------------------------------------------------------------===;
___divsi3:
	; Result sign = XOR of dividend sign and divisor sign
	; Divisor high byte is on stack. After call to ___divsi3, stack is:
	;   SP+0 = return addr (2)
	;   SP+2 = divisor byte 0 (lo)
	;   SP+3 = divisor byte 1
	;   SP+4 = divisor byte 2
	;   SP+5 = divisor byte 3 (hi, sign bit)
	ldhl	sp, #5
	ld	a, (hl)		; A = divisor high byte
	xor	d		; bit 7 = result sign
	push	af		; save result sign

	; Make dividend positive (if negative)
	bit	7, d
	call	nz, __neg32_debc

	; Make divisor positive (if negative, on stack)
	; After push af, divisor is at SP+4..SP+7
	ldhl	sp, #7
	bit	7, (hl)
	jr	z, ___divsi3_div_pos
	ldhl	sp, #4
	call	__neg32_on_stack
___divsi3_div_pos:

	; Unsigned division core
	push	de		; quotient hi = |dividend| hi
	push	bc		; quotient lo = |dividend| lo
	ld	d, #0
	ld	e, d
	ld	b, d
	ld	c, d
	ld	a, #32
	push	af
	call	__udiv32_loop
	pop	af		; counter
	pop	bc		; quotient lo
	pop	de		; quotient hi

	; Apply result sign
	pop	af		; result sign (bit 7)
	bit	7, a
	call	nz, __neg32_debc
	pop	hl		; return address
	add	sp, #4		; callee-cleanup: skip 4 bytes of stack args
	jp	(hl)

;===------------------------------------------------------------------------===;
; ___modsi3 - Signed 32-bit modulo
;
; Input:  DEBC = dividend, stack+2..+5 = divisor
; Output: DEBC = remainder (sign follows dividend)
;===------------------------------------------------------------------------===;
___modsi3:
	; Remainder sign = dividend sign
	ld	a, d
	push	af		; save dividend sign (bit 7)

	; Make dividend positive
	bit	7, d
	call	nz, __neg32_debc

	; Make divisor positive (on stack, after push af: SP+4..SP+7)
	ldhl	sp, #7
	bit	7, (hl)
	jr	z, ___modsi3_div_pos
	ldhl	sp, #4
	call	__neg32_on_stack
___modsi3_div_pos:

	; Unsigned division core
	push	de
	push	bc
	ld	d, #0
	ld	e, d
	ld	b, d
	ld	c, d
	ld	a, #32
	push	af
	call	__udiv32_loop
	pop	af		; counter
	add	sp, #4		; discard quotient

	; Apply dividend sign to remainder
	pop	af		; dividend sign
	bit	7, a
	call	nz, __neg32_debc
	pop	hl		; return address
	add	sp, #4		; callee-cleanup: skip 4 bytes of stack args
	jp	(hl)

;===------------------------------------------------------------------------===;
;=== 64-bit Integer Arithmetic ===============================================;
;===------------------------------------------------------------------------===;
;
; 64-bit division and modulo via restoring division algorithm.
; SM83 adaptation: no IX/IY, uses ldhl sp,#n for stack-relative access.
;
; Calling convention (sret demotion, SDCC __sdcccall(1)):
;   Stack (caller pushes): sret pointer (2B), dividend (8B), divisor (8B)
;   sret pointer is at lowest address (pushed last by caller)
;
; Stack frame after push af; add sp,#-24:
;   SP+0..SP+7   = quotient (8 bytes, little-endian)
;   SP+8..SP+15  = remainder (8 bytes, little-endian)
;   SP+16..SP+23 = divisor copy (8 bytes)
;   SP+24,SP+25  = saved AF (sign/dummy)
;   SP+26,SP+27  = return address
;   SP+28,SP+29  = sret pointer (caller pushed on stack)
;   SP+30..SP+37 = dividend (8 bytes, from caller)
;   SP+38..SP+45 = divisor (8 bytes, from caller)
;
;===------------------------------------------------------------------------===;

;===------------------------------------------------------------------------===;
; __neg64_mem - Negate 8-byte value in memory (two's complement)
; Input: HL = pointer to 8-byte little-endian value
; Clobbers: A, DE, HL
