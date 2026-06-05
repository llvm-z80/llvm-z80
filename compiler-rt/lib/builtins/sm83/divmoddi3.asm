; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl __neg64_mem
	.globl __udiv64_setup_sm83
	.globl __udiv64_core_sm83
	.globl __udiv64_loop_sm83
	.globl __udiv64_dosub_sm83
	.globl __udiv64_skip_sm83
	.globl __udiv64_copy_quot_sm83
	.globl __udiv64_copy_rem_sm83
	.globl ___udivdi3
	.globl ___umoddi3
	.globl ___divdi3
	.globl ___divdi3_dp
	.globl ___divdi3_vp
	.globl ___divdi3_done
	.globl ___moddi3
	.globl ___moddi3_dp
	.globl ___moddi3_vp
	.globl ___moddi3_done

__neg64_mem:
	; Complement all 8 bytes
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
	ld	(hl+), a
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
	; HL points to byte 7; rewind to byte 0
	ld	de, #-7
	add	hl, de
	; Add 1 (increment 8-byte LE value)
	inc	(hl)
	ret	nz
	inc	hl
	inc	(hl)
	ret	nz
	inc	hl
	inc	(hl)
	ret	nz
	inc	hl
	inc	(hl)
	ret	nz
	inc	hl
	inc	(hl)
	ret	nz
	inc	hl
	inc	(hl)
	ret	nz
	inc	hl
	inc	(hl)
	ret	nz
	inc	hl
	inc	(hl)
	ret

;===------------------------------------------------------------------------===;
; __udiv64_setup_sm83 - Initialize quotient, remainder, and divisor copy
;
; Called from main 64-bit div/mod functions.
; Stack layout inside (call adds +2):
;   SP+2..SP+9   = quotient
;   SP+10..SP+17 = remainder
;   SP+18..SP+25 = divisor copy
;   SP+32..SP+39 = dividend (on caller's stack)
;   SP+40..SP+47 = divisor (on caller's stack)
;===------------------------------------------------------------------------===;
__udiv64_setup_sm83:
	; DE → dividend[0] (SP+32), HL → quotient[0] (SP+2)
	ldhl	sp, #32
	ld	d, h
	ld	e, l
	ldhl	sp, #2
	; Copy 8 bytes: dividend → quotient
	ld	a, (de)
	ld	(hl+), a
	inc	de
	ld	a, (de)
	ld	(hl+), a
	inc	de
	ld	a, (de)
	ld	(hl+), a
	inc	de
	ld	a, (de)
	ld	(hl+), a
	inc	de
	ld	a, (de)
	ld	(hl+), a
	inc	de
	ld	a, (de)
	ld	(hl+), a
	inc	de
	ld	a, (de)
	ld	(hl+), a
	inc	de
	ld	a, (de)
	ld	(hl+), a
	inc	de
	; HL → remainder[0] (SP+10), DE → divisor[0] (SP+40)
	; Zero 8 bytes: remainder
	xor	a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	; HL → div_copy[0] (SP+18), DE → divisor[0] (SP+40)
	; Copy 8 bytes: divisor → divisor copy
	ld	a, (de)
	ld	(hl+), a
	inc	de
	ld	a, (de)
	ld	(hl+), a
	inc	de
	ld	a, (de)
	ld	(hl+), a
	inc	de
	ld	a, (de)
	ld	(hl+), a
	inc	de
	ld	a, (de)
	ld	(hl+), a
	inc	de
	ld	a, (de)
	ld	(hl+), a
	inc	de
	ld	a, (de)
	ld	(hl+), a
	inc	de
	ld	a, (de)
	ld	(hl+), a
	ret

;===------------------------------------------------------------------------===;
; __udiv64_core_sm83 - Core 64-bit unsigned division (restoring algorithm)
;
; B = 64 (loop counter, set by caller before call)
; Stack layout inside (call adds +2):
;   SP+2..SP+9   = quotient (8 bytes)
;   SP+10..SP+17 = remainder (8 bytes)
;   SP+18..SP+25 = divisor copy (8 bytes)
;
; Uses jp nz for loop (body exceeds jr range).
;===------------------------------------------------------------------------===;
__udiv64_core_sm83:

__udiv64_loop_sm83:
	; Step 1: Shift quotient left by 1 (8 bytes)
	ldhl	sp, #2
	sla	(hl)		; quotient[0]
	inc	hl
	rl	(hl)		; quotient[1]
	inc	hl
	rl	(hl)		; quotient[2]
	inc	hl
	rl	(hl)		; quotient[3]
	inc	hl
	rl	(hl)		; quotient[4]
	inc	hl
	rl	(hl)		; quotient[5]
	inc	hl
	rl	(hl)		; quotient[6]
	inc	hl
	rl	(hl)		; quotient[7], carry = MSB

	; Step 2: Shift carry into remainder (8 bytes, adjacent)
	; inc hl does NOT affect flags — carry preserved from quotient
	inc	hl		; HL → remainder[0]
	rl	(hl)
	inc	hl
	rl	(hl)
	inc	hl
	rl	(hl)
	inc	hl
	rl	(hl)
	inc	hl
	rl	(hl)
	inc	hl
	rl	(hl)
	inc	hl
	rl	(hl)
	inc	hl
	rl	(hl)		; carry = remainder overflow

	; Step 3: If remainder overflowed → must subtract
	jr	c, __udiv64_dosub_sm83

	; Step 4: Compare remainder >= divisor (non-destructive)
	; DE → div_copy[0] (SP+18), HL → remainder[0] (SP+10)
	ldhl	sp, #18
	ld	d, h
	ld	e, l
	ldhl	sp, #10
	; Byte 0: sub (no carry in — ldhl cleared carry state)
	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl+)
	sub	c
	; Bytes 1-7: sbc (carry chain preserved — ld/inc don't affect flags)
	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl+)
	sbc	a, c

	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl+)
	sbc	a, c

	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl+)
	sbc	a, c

	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl+)
	sbc	a, c

	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl+)
	sbc	a, c

	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl+)
	sbc	a, c

	ld	a, (de)
	ld	c, a
	ld	a, (hl)
	sbc	a, c
	; If carry: remainder < divisor → skip
	jr	c, __udiv64_skip_sm83

__udiv64_dosub_sm83:
	; Step 5: Subtract divisor from remainder
	; DE → div_copy[0] (SP+18), HL → remainder[0] (SP+10)
	ldhl	sp, #18
	ld	d, h
	ld	e, l
	ldhl	sp, #10
	; Byte 0: sub
	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl)
	sub	c
	ld	(hl+), a
	; Bytes 1-7: sbc
	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl)
	sbc	a, c
	ld	(hl+), a

	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl)
	sbc	a, c
	ld	(hl+), a

	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl)
	sbc	a, c
	ld	(hl+), a

	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl)
	sbc	a, c
	ld	(hl+), a

	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl)
	sbc	a, c
	ld	(hl+), a

	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl)
	sbc	a, c
	ld	(hl+), a

	ld	a, (de)
	ld	c, a
	ld	a, (hl)
	sbc	a, c
	ld	(hl), a

	; Set quotient bit 0
	ldhl	sp, #2
	inc	(hl)

__udiv64_skip_sm83:
	dec	b
	jp	nz, __udiv64_loop_sm83
	ret

;===------------------------------------------------------------------------===;
; __udiv64_copy_quot_sm83 - Copy quotient to sret pointer
;
; Stack inside (call adds +2):
;   SP+2..SP+9 = quotient, SP+30,31 = sret
;===------------------------------------------------------------------------===;
__udiv64_copy_quot_sm83:
	; Load sret pointer from stack
	ldhl	sp, #30
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)		; DE = sret pointer
	; Copy quotient → sret
	ldhl	sp, #2		; HL → quotient[0]
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
	ret

;===------------------------------------------------------------------------===;
; __udiv64_copy_rem_sm83 - Copy remainder to sret pointer
;
; Stack inside (call adds +2):
;   SP+10..SP+17 = remainder, SP+30,31 = sret
;===------------------------------------------------------------------------===;
__udiv64_copy_rem_sm83:
	; Load sret pointer from stack
	ldhl	sp, #30
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)		; DE = sret pointer
	; Copy remainder → sret
	ldhl	sp, #10		; HL → remainder[0]
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
	ret

;===------------------------------------------------------------------------===;
; ___udivdi3 - Unsigned 64-bit division
;
; Input:  stack = sret pointer (2B) + dividend (8B) + divisor (8B)
; Output: writes 8-byte quotient to sret pointer
;===------------------------------------------------------------------------===;
___udivdi3:
	push	af		; dummy (uniform frame with signed variants)
	add	sp, #-24	; allocate quotient(8) + remainder(8) + div_copy(8)

	call	__udiv64_setup_sm83
	ld	b, #64
	call	__udiv64_core_sm83
	call	__udiv64_copy_quot_sm83

	add	sp, #26		; cleanup: 24 + 2 (dummy)
	pop	hl		; return address
	add	sp, #18		; callee-cleanup: skip sret(2) + args(16)
	jp	(hl)

;===------------------------------------------------------------------------===;
; ___umoddi3 - Unsigned 64-bit modulo
;
; Input:  stack = sret pointer (2B) + dividend (8B) + divisor (8B)
; Output: writes 8-byte remainder to sret pointer
;===------------------------------------------------------------------------===;
___umoddi3:
	push	af		; dummy
	add	sp, #-24

	call	__udiv64_setup_sm83
	ld	b, #64
	call	__udiv64_core_sm83
	call	__udiv64_copy_rem_sm83

	add	sp, #26
	pop	hl		; return address
	add	sp, #18		; callee-cleanup: skip sret(2) + args(16)
	jp	(hl)

;===------------------------------------------------------------------------===;
; ___divdi3 - Signed 64-bit division
;
; Input:  stack = sret pointer (2B) + dividend (8B) + divisor (8B)
; Output: writes 8-byte quotient to sret pointer (sign = XOR of input signs)
;===------------------------------------------------------------------------===;
___divdi3:
	; SP+0,1=ret_addr, SP+2,3=sret, SP+4..11=dividend, SP+12..19=divisor

	; Compute result sign = dividend_sign XOR divisor_sign
	ldhl	sp, #11		; HL → dividend MSB
	ld	a, (hl)
	ldhl	sp, #19		; HL → divisor MSB
	xor	(hl)
	ld	c, a		; C = result sign (bit 7)

	; Negate dividend if negative
	ldhl	sp, #11
	bit	7, (hl)
	jr	z, ___divdi3_dp
	ldhl	sp, #4
	call	__neg64_mem
___divdi3_dp:

	; Negate divisor if negative
	ldhl	sp, #19
	bit	7, (hl)
	jr	z, ___divdi3_vp
	ldhl	sp, #12
	call	__neg64_mem
___divdi3_vp:

	; Push sign, allocate local space
	ld	a, c
	push	af		; sign
	add	sp, #-24	; allocate
	; Layout: SP+0..7=quot, SP+8..15=rem, SP+16..23=div_copy
	;         SP+24,25=sign, SP+26,27=ret_addr, SP+28,29=sret

	call	__udiv64_setup_sm83
	ld	b, #64
	call	__udiv64_core_sm83
	call	__udiv64_copy_quot_sm83

	add	sp, #24		; free local space
	pop	af		; sign

	; Negate result if sign bit set
	bit	7, a
	jr	z, ___divdi3_done
	; Load sret pointer from stack (SP+2,3 = sret)
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = sret pointer
	call	__neg64_mem
___divdi3_done:
	pop	hl		; return address
	add	sp, #18		; callee-cleanup: skip sret(2) + args(16)
	jp	(hl)

;===------------------------------------------------------------------------===;
; ___moddi3 - Signed 64-bit modulo
;
; Input:  stack = sret pointer (2B) + dividend (8B) + divisor (8B)
; Output: writes 8-byte remainder to sret pointer (sign follows dividend)
;===------------------------------------------------------------------------===;
___moddi3:
	; SP+0,1=ret_addr, SP+2,3=sret, SP+4..11=dividend, SP+12..19=divisor

	; Remainder sign = dividend sign
	ldhl	sp, #11		; HL → dividend MSB
	ld	c, (hl)		; C = dividend sign byte (bit 7 = sign)

	; Negate dividend if negative
	bit	7, c
	jr	z, ___moddi3_dp
	ldhl	sp, #4
	call	__neg64_mem
___moddi3_dp:

	; Negate divisor if negative
	ldhl	sp, #19
	bit	7, (hl)
	jr	z, ___moddi3_vp
	ldhl	sp, #12
	call	__neg64_mem
___moddi3_vp:

	; Push sign, allocate local space
	ld	a, c
	push	af		; sign
	add	sp, #-24	; allocate

	call	__udiv64_setup_sm83
	ld	b, #64
	call	__udiv64_core_sm83
	call	__udiv64_copy_rem_sm83

	add	sp, #24		; free local space
	pop	af		; sign

	; Negate result if dividend was negative
	bit	7, a
	jr	z, ___moddi3_done
	; Load sret pointer from stack (SP+2,3 = sret)
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = sret pointer
	call	__neg64_mem
___moddi3_done:
	pop	hl		; return address
	add	sp, #18		; callee-cleanup: skip sret(2) + args(16)
	jp	(hl)

;===------------------------------------------------------------------------===;
;
;  IEEE 754 Single-Precision Floating Point (softfloat) for SM83
;
;===------------------------------------------------------------------------===;
;
; SM83 f32 representation in registers:
;   DEBC = float (D=sign+exp_hi, E=exp_lo+mant_hi, B=mant_mid, C=mant_lo)
;
; Calling convention (__sdcccall(1) for SM83):
;   1st f32 arg → DEBC,  2nd f32 arg → stack
;   f32 return  → DEBC,  i16 return  → BC
;
; Stack layout at function entry (two-arg float functions):
;   SP+0: return address (2 bytes)
;   SP+2: arg2 byte 0 (C2, LSB)
;   SP+3: arg2 byte 1 (B2)
;   SP+4: arg2 byte 2 (E2)
;   SP+5: arg2 byte 3 (D2, MSB)
;
; SM83 lacks IX/IY — uses LDHL SP,n for stack-relative access.
;

;===------------------------------------------------------------------------===;
; ___unordsf2 - Check if either argument is NaN
;
; Input:  DEBC = a, stack = b
; Output: BC = nonzero (1) if a or b is NaN, 0 if both ordered
