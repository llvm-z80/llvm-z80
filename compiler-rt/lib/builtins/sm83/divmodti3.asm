; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl __neg128_mem
	.globl __udiv128_setup_sm83
	.globl __udiv128_core_sm83
	.globl __udiv128_loop_sm83
	.globl __udiv128_dosub_sm83
	.globl __udiv128_skip_sm83
	.globl __udiv128_copy_quot_sm83
	.globl __udiv128_copy_rem_sm83
	.globl ___udivti3
	.globl ___umodti3
	.globl ___divti3
	.globl ___divti3_dp
	.globl ___divti3_vp
	.globl ___divti3_done
	.globl ___modti3
	.globl ___modti3_dp
	.globl ___modti3_vp
	.globl ___modti3_done

;===------------------------------------------------------------------------===;
; SM83 128-bit Integer Division and Modulo
;
; Restoring division algorithm, fully memory-based (no IX/IY on SM83).
;
; Calling convention (sret demotion, SDCC __sdcccall(1)):
;   Stack: return address (2B), sret pointer (2B), dividend (16B), divisor (16B)
;
; For unsigned functions, stack inside after push af + allocate:
;   SP+0..SP+15   = quotient  (16 bytes)
;   SP+16..SP+31  = remainder (16 bytes)
;   SP+32..SP+47  = divisor copy (16 bytes)
;   SP+48,49      = dummy (push af)
;   SP+50,51      = return address
;   SP+52,53      = sret pointer
;   SP+54..SP+69  = dividend (16 bytes)
;   SP+70..SP+85  = divisor  (16 bytes)
;
; Note: function calls add +2 to all SP offsets within subroutines.
;===------------------------------------------------------------------------===;

;===------------------------------------------------------------------------===;
; __neg128_mem - Negate 16-byte value in memory (two's complement)
; Input: HL = pointer to 16-byte little-endian value
; Clobbers: A, DE, HL
; NOTE: This is a WEAK definition. If divmoddi3 provides __neg64_mem,
;       they coexist since __neg128_mem is a separate symbol.
;===------------------------------------------------------------------------===;
__neg128_mem:
	; Complement all 16 bytes
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
	ld	(hl+), a
	ld	a, (hl)
	cpl
	ld	(hl), a
	; HL points to byte 15; rewind to byte 0
	ld	de, #-15
	add	hl, de
	; Add 1 (increment 16-byte LE value)
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
	ret	nz
	inc	hl
	inc	(hl)
	ret

;===------------------------------------------------------------------------===;
; __udiv128_setup_sm83 - Initialize quotient, remainder, and divisor copy
;
; Called from main 128-bit div/mod functions.
; Stack layout inside (call adds +2):
;   SP+2..SP+17   = quotient  (16 bytes)
;   SP+18..SP+33  = remainder (16 bytes)
;   SP+34..SP+49  = divisor copy (16 bytes)
;   SP+56..SP+71  = dividend (on caller's stack)
;   SP+72..SP+87  = divisor  (on caller's stack)
;===------------------------------------------------------------------------===;
__udiv128_setup_sm83:
	; DE → dividend[0] (SP+56), HL → quotient[0] (SP+2)
	ldhl	sp, #56
	ld	d, h
	ld	e, l
	ldhl	sp, #2
	; Copy 16 bytes: dividend → quotient
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
	; HL → remainder[0] (SP+18), DE past dividend
	; Zero 16 bytes: remainder
	xor	a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	; HL → div_copy[0] (SP+34), DE → divisor[0] (SP+72)
	ldhl	sp, #72
	ld	d, h
	ld	e, l
	ldhl	sp, #34
	; Copy 16 bytes: divisor → divisor copy
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
; __udiv128_core_sm83 - Core 128-bit unsigned division (restoring algorithm)
;
; B = loop counter (set by caller, 128 total via two groups of 64)
; C = outer loop counter
; Stack layout inside (call adds +2):
;   SP+2..SP+17   = quotient  (16 bytes)
;   SP+18..SP+33  = remainder (16 bytes)
;   SP+34..SP+49  = divisor copy (16 bytes)
;===------------------------------------------------------------------------===;
__udiv128_core_sm83:

__udiv128_loop_sm83:
	; Step 1: Shift quotient left by 1 (16 bytes)
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
	rl	(hl)		; quotient[7]
	inc	hl
	rl	(hl)		; quotient[8]
	inc	hl
	rl	(hl)		; quotient[9]
	inc	hl
	rl	(hl)		; quotient[10]
	inc	hl
	rl	(hl)		; quotient[11]
	inc	hl
	rl	(hl)		; quotient[12]
	inc	hl
	rl	(hl)		; quotient[13]
	inc	hl
	rl	(hl)		; quotient[14]
	inc	hl
	rl	(hl)		; quotient[15], carry = MSB

	; Step 2: Shift carry into remainder (16 bytes, adjacent)
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
	rl	(hl)
	inc	hl
	rl	(hl)		; carry = remainder overflow

	; Step 3: If remainder overflowed → must subtract
	jp	c, __udiv128_dosub_sm83

	; Step 4: Compare remainder >= divisor (non-destructive)
	; DE → div_copy[0] (SP+34), HL → remainder[0] (SP+18)
	ldhl	sp, #34
	ld	d, h
	ld	e, l
	ldhl	sp, #18
	; Byte 0: sub
	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl+)
	sub	c
	; Bytes 1-15: sbc
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
	jp	c, __udiv128_skip_sm83

__udiv128_dosub_sm83:
	; Step 5: Subtract divisor from remainder
	; DE → div_copy[0] (SP+34), HL → remainder[0] (SP+18)
	ldhl	sp, #34
	ld	d, h
	ld	e, l
	ldhl	sp, #18
	; Byte 0: sub
	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (hl)
	sub	c
	ld	(hl+), a
	; Bytes 1-15: sbc
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

__udiv128_skip_sm83:
	dec	b
	jp	nz, __udiv128_loop_sm83
	ret

;===------------------------------------------------------------------------===;
; __udiv128_copy_quot_sm83 - Copy quotient to sret pointer
;
; Stack inside (call adds +2):
;   SP+2..SP+17 = quotient, SP+54,55 = sret
;===------------------------------------------------------------------------===;
__udiv128_copy_quot_sm83:
	; Load sret pointer from stack
	ldhl	sp, #54
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
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl)
	ld	(de), a
	ret

;===------------------------------------------------------------------------===;
; __udiv128_copy_rem_sm83 - Copy remainder to sret pointer
;
; Stack inside (call adds +2):
;   SP+18..SP+33 = remainder, SP+54,55 = sret
;===------------------------------------------------------------------------===;
__udiv128_copy_rem_sm83:
	; Load sret pointer from stack
	ldhl	sp, #54
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)		; DE = sret pointer
	; Copy remainder → sret
	ldhl	sp, #18		; HL → remainder[0]
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
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl)
	ld	(de), a
	ret

;===------------------------------------------------------------------------===;
; ___udivti3 - Unsigned 128-bit division
;
; Input:  stack = sret pointer (2B) + dividend (16B) + divisor (16B)
; Output: writes 16-byte quotient to sret pointer
;===------------------------------------------------------------------------===;
___udivti3:
	push	af		; dummy (uniform frame with signed variants)
	add	sp, #-48	; allocate quotient(16) + remainder(16) + div_copy(16)

	call	__udiv128_setup_sm83
	ld	b, #64
	call	__udiv128_core_sm83
	ld	b, #64
	call	__udiv128_core_sm83
	call	__udiv128_copy_quot_sm83

	add	sp, #50		; cleanup: 48 + 2 (dummy); SM83 ADD SP range is -128..+127
	pop	hl		; return address
	add	sp, #34		; callee-cleanup: skip sret(2) + args(32)
	jp	(hl)

;===------------------------------------------------------------------------===;
; ___umodti3 - Unsigned 128-bit modulo
;
; Input:  stack = sret pointer (2B) + dividend (16B) + divisor (16B)
; Output: writes 16-byte remainder to sret pointer
;===------------------------------------------------------------------------===;
___umodti3:
	push	af		; dummy
	add	sp, #-48

	call	__udiv128_setup_sm83
	ld	b, #64
	call	__udiv128_core_sm83
	ld	b, #64
	call	__udiv128_core_sm83
	call	__udiv128_copy_rem_sm83

	add	sp, #50
	pop	hl		; return address
	add	sp, #34		; callee-cleanup: skip sret(2) + args(32)
	jp	(hl)

;===------------------------------------------------------------------------===;
; ___divti3 - Signed 128-bit division
;
; Input:  stack = sret pointer (2B) + dividend (16B) + divisor (16B)
; Output: writes 16-byte quotient to sret pointer (sign = XOR of input signs)
;===------------------------------------------------------------------------===;
___divti3:
	; SP+0,1=ret_addr, SP+2,3=sret, SP+4..19=dividend, SP+20..35=divisor

	; Compute result sign = dividend_sign XOR divisor_sign
	ldhl	sp, #19		; HL → dividend MSB
	ld	a, (hl)
	ldhl	sp, #35		; HL → divisor MSB
	xor	(hl)
	ld	c, a		; C = result sign (bit 7)

	; Negate dividend if negative
	ldhl	sp, #19
	bit	7, (hl)
	jr	z, ___divti3_dp
	ldhl	sp, #4
	call	__neg128_mem
___divti3_dp:

	; Negate divisor if negative
	ldhl	sp, #35
	bit	7, (hl)
	jr	z, ___divti3_vp
	ldhl	sp, #20
	call	__neg128_mem
___divti3_vp:

	; Push sign, allocate local space
	ld	a, c
	push	af		; sign
	add	sp, #-48	; allocate

	call	__udiv128_setup_sm83
	ld	b, #64
	call	__udiv128_core_sm83
	ld	b, #64
	call	__udiv128_core_sm83
	call	__udiv128_copy_quot_sm83

	add	sp, #48		; free local space
	pop	af		; sign

	; Negate result if sign bit set
	bit	7, a
	jr	z, ___divti3_done
	; Load sret pointer from stack (SP+2,3 = sret)
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = sret pointer
	call	__neg128_mem
___divti3_done:
	pop	hl		; return address
	add	sp, #34		; callee-cleanup: skip sret(2) + args(32)
	jp	(hl)

;===------------------------------------------------------------------------===;
; ___modti3 - Signed 128-bit modulo
;
; Input:  stack = sret pointer (2B) + dividend (16B) + divisor (16B)
; Output: writes 16-byte remainder to sret pointer (sign follows dividend)
;===------------------------------------------------------------------------===;
___modti3:
	; SP+0,1=ret_addr, SP+2,3=sret, SP+4..19=dividend, SP+20..35=divisor

	; Remainder sign = dividend sign
	ldhl	sp, #19		; HL → dividend MSB
	ld	c, (hl)		; C = dividend sign byte (bit 7 = sign)

	; Negate dividend if negative
	bit	7, c
	jr	z, ___modti3_dp
	ldhl	sp, #4
	call	__neg128_mem
___modti3_dp:

	; Negate divisor if negative
	ldhl	sp, #35
	bit	7, (hl)
	jr	z, ___modti3_vp
	ldhl	sp, #20
	call	__neg128_mem
___modti3_vp:

	; Push sign, allocate local space
	ld	a, c
	push	af		; sign
	add	sp, #-48	; allocate

	call	__udiv128_setup_sm83
	ld	b, #64
	call	__udiv128_core_sm83
	ld	b, #64
	call	__udiv128_core_sm83
	call	__udiv128_copy_rem_sm83

	add	sp, #48		; free local space
	pop	af		; sign

	; Negate result if dividend was negative
	bit	7, a
	jr	z, ___modti3_done
	; Load sret pointer from stack (SP+2,3 = sret)
	ldhl	sp, #2
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = sret pointer
	call	__neg128_mem
___modti3_done:
	pop	hl		; return address
	add	sp, #34		; callee-cleanup: skip sret(2) + args(32)
	jp	(hl)
