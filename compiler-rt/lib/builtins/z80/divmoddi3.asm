; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl __udiv64_setup
	.globl __udiv64_core
	.globl __udiv64_loop
	.globl __udiv64_skip
	.globl __udiv64_copy_quot
	.globl __udiv64_copy_rem
	.globl __neg64_mem
	.globl ___udivdi3
	.globl ___divdi3
	.globl __divdi3_div_pos
	.globl __divdi3_dvs_pos
	.globl __divdi3_done
	.globl ___umoddi3
	.globl ___moddi3
	.globl __moddi3_div_pos
	.globl __moddi3_dvs_pos
	.globl __moddi3_done

__udiv64_setup:
	; Copy dividend (IX+6..IX+13) to quotient (IX-8..IX-1)
	ld	a, 6(ix)
	ld	-8(ix), a
	ld	a, 7(ix)
	ld	-7(ix), a
	ld	a, 8(ix)
	ld	-6(ix), a
	ld	a, 9(ix)
	ld	-5(ix), a
	ld	a, 10(ix)
	ld	-4(ix), a
	ld	a, 11(ix)
	ld	-3(ix), a
	ld	a, 12(ix)
	ld	-2(ix), a
	ld	a, 13(ix)
	ld	-1(ix), a

	; Zero remainder (IX-16..IX-9)
	xor	a
	ld	-16(ix), a
	ld	-15(ix), a
	ld	-14(ix), a
	ld	-13(ix), a
	ld	-12(ix), a
	ld	-11(ix), a
	ld	-10(ix), a
	ld	-9(ix), a

	ret

;===------------------------------------------------------------------------===;
; __udiv64_core - Core 64-bit unsigned division (restoring algorithm)
;
; Performs 64 iterations of shift-compare-subtract.
; Uses dec b + jp nz (DJNZ range insufficient for ~192-byte loop body).
;===------------------------------------------------------------------------===;
__udiv64_core:
	ld	b, #64

__udiv64_loop:
	; Step 1: Shift quotient left by 1 (8 bytes, LSB at IX-8)
	sla	-8(ix)
	rl	-7(ix)
	rl	-6(ix)
	rl	-5(ix)
	rl	-4(ix)
	rl	-3(ix)
	rl	-2(ix)
	rl	-1(ix)

	; Step 2: Shift carry into remainder (8 bytes, LSB at IX-16)
	rl	-16(ix)
	rl	-15(ix)
	rl	-14(ix)
	rl	-13(ix)
	rl	-12(ix)
	rl	-11(ix)
	rl	-10(ix)
	rl	-9(ix)

	; Step 3: Compare remainder >= divisor (non-destructive)
	ld	a, -16(ix)
	sub	14(ix)
	ld	a, -15(ix)
	sbc	a, 15(ix)
	ld	a, -14(ix)
	sbc	a, 16(ix)
	ld	a, -13(ix)
	sbc	a, 17(ix)
	ld	a, -12(ix)
	sbc	a, 18(ix)
	ld	a, -11(ix)
	sbc	a, 19(ix)
	ld	a, -10(ix)
	sbc	a, 20(ix)
	ld	a, -9(ix)
	sbc	a, 21(ix)
	jr	c, __udiv64_skip	; remainder < divisor, skip

	; Step 4: remainder >= divisor: subtract divisor from remainder
	ld	a, -16(ix)
	sub	14(ix)
	ld	-16(ix), a
	ld	a, -15(ix)
	sbc	a, 15(ix)
	ld	-15(ix), a
	ld	a, -14(ix)
	sbc	a, 16(ix)
	ld	-14(ix), a
	ld	a, -13(ix)
	sbc	a, 17(ix)
	ld	-13(ix), a
	ld	a, -12(ix)
	sbc	a, 18(ix)
	ld	-12(ix), a
	ld	a, -11(ix)
	sbc	a, 19(ix)
	ld	-11(ix), a
	ld	a, -10(ix)
	sbc	a, 20(ix)
	ld	-10(ix), a
	ld	a, -9(ix)
	sbc	a, 21(ix)
	ld	-9(ix), a

	; Set quotient bit 0
	set	0, -8(ix)

__udiv64_skip:
	dec	b
	jp	nz, __udiv64_loop
	ret

;===------------------------------------------------------------------------===;
; __udiv64_copy_quot - Copy quotient to sret pointer
; Copies IX-8..IX-1 to address at IX+4,5
;===------------------------------------------------------------------------===;
__udiv64_copy_quot:
	ld	l, 4(ix)
	ld	h, 5(ix)
	ld	a, -8(ix)
	ld	(hl), a
	inc	hl
	ld	a, -7(ix)
	ld	(hl), a
	inc	hl
	ld	a, -6(ix)
	ld	(hl), a
	inc	hl
	ld	a, -5(ix)
	ld	(hl), a
	inc	hl
	ld	a, -4(ix)
	ld	(hl), a
	inc	hl
	ld	a, -3(ix)
	ld	(hl), a
	inc	hl
	ld	a, -2(ix)
	ld	(hl), a
	inc	hl
	ld	a, -1(ix)
	ld	(hl), a
	ret

;===------------------------------------------------------------------------===;
; __udiv64_copy_rem - Copy remainder to sret pointer
; Copies IX-16..IX-9 to address at IX+4,5
;===------------------------------------------------------------------------===;
__udiv64_copy_rem:
	ld	l, 4(ix)
	ld	h, 5(ix)
	ld	a, -16(ix)
	ld	(hl), a
	inc	hl
	ld	a, -15(ix)
	ld	(hl), a
	inc	hl
	ld	a, -14(ix)
	ld	(hl), a
	inc	hl
	ld	a, -13(ix)
	ld	(hl), a
	inc	hl
	ld	a, -12(ix)
	ld	(hl), a
	inc	hl
	ld	a, -11(ix)
	ld	(hl), a
	inc	hl
	ld	a, -10(ix)
	ld	(hl), a
	inc	hl
	ld	a, -9(ix)
	ld	(hl), a
	ret

;===------------------------------------------------------------------------===;
; __neg64_mem - Negate 8-byte value in memory (two's complement)
; Input: HL = pointer to 8-byte little-endian value
; Clobbers: A, DE, HL
;===------------------------------------------------------------------------===;
__neg64_mem:
	; Complement all 8 bytes
	ld	a, (hl)
	cpl
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	cpl
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	cpl
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	cpl
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	cpl
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	cpl
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	cpl
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	cpl
	ld	(hl), a
	; HL now points to byte 7; rewind to byte 0
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
; ___udivdi3 - Unsigned 64-bit division
;
; Input:  stack = sret pointer (2B) + dividend (8B) + divisor (8B)
; Output: writes 8-byte quotient to sret pointer
;===------------------------------------------------------------------------===;
___udivdi3:
	push	ix
	ld	ix, #0
	add	ix, sp
	; Allocate 16 bytes for quotient(8) + remainder(8)
	ld	hl, #-16
	add	hl, sp
	ld	sp, hl

	call	__udiv64_setup
	call	__udiv64_core
	call	__udiv64_copy_quot

	ld	sp, ix
	pop	ix
	ret			; caller cleanup (i64 return > 16 bits)

;===------------------------------------------------------------------------===;
; ___divdi3 - Signed 64-bit division
;
; Input:  stack = sret pointer (2B) + dividend (8B) + divisor (8B)
; Output: writes 8-byte quotient to sret pointer
;===------------------------------------------------------------------------===;
___divdi3:
	push	ix
	ld	ix, #0
	add	ix, sp
	; Allocate 16 bytes for quotient(8) + remainder(8)
	ld	hl, #-16
	add	hl, sp
	ld	sp, hl

	; Result sign = XOR of input signs
	ld	a, 13(ix)	; MSB of dividend (sign byte)
	xor	21(ix)		; XOR with MSB of divisor
	push	af		; save result sign (bit 7)

	; Make dividend positive if negative
	bit	7, 13(ix)
	jr	z, __divdi3_div_pos
	push	ix
	pop	hl
	ld	de, #6
	add	hl, de		; HL = &dividend
	call	__neg64_mem
__divdi3_div_pos:

	; Make divisor positive if negative
	bit	7, 21(ix)
	jr	z, __divdi3_dvs_pos
	push	ix
	pop	hl
	ld	de, #14
	add	hl, de		; HL = &divisor
	call	__neg64_mem
__divdi3_dvs_pos:

	call	__udiv64_setup
	call	__udiv64_core
	call	__udiv64_copy_quot

	; Apply result sign
	pop	af
	bit	7, a
	jr	z, __divdi3_done
	; Negate result at sret pointer
	ld	l, 4(ix)
	ld	h, 5(ix)
	call	__neg64_mem
__divdi3_done:

	ld	sp, ix
	pop	ix
	ret			; caller cleanup (i64 return > 16 bits)

;===------------------------------------------------------------------------===;
; ___umoddi3 - Unsigned 64-bit modulo
;
; Input:  stack = sret pointer (2B) + dividend (8B) + divisor (8B)
; Output: writes 8-byte remainder to sret pointer
;===------------------------------------------------------------------------===;
___umoddi3:
	push	ix
	ld	ix, #0
	add	ix, sp
	; Allocate 16 bytes for quotient(8) + remainder(8)
	ld	hl, #-16
	add	hl, sp
	ld	sp, hl

	call	__udiv64_setup
	call	__udiv64_core
	call	__udiv64_copy_rem

	ld	sp, ix
	pop	ix
	ret			; caller cleanup (i64 return > 16 bits)

;===------------------------------------------------------------------------===;
; ___moddi3 - Signed 64-bit modulo
;
; Input:  stack = sret pointer (2B) + dividend (8B) + divisor (8B)
; Output: writes 8-byte remainder to sret pointer (sign follows dividend)
;===------------------------------------------------------------------------===;
___moddi3:
	push	ix
	ld	ix, #0
	add	ix, sp
	; Allocate 16 bytes for quotient(8) + remainder(8)
	ld	hl, #-16
	add	hl, sp
	ld	sp, hl

	; Remainder sign = dividend sign
	ld	a, 13(ix)	; MSB of dividend (sign byte)
	push	af		; save remainder sign (bit 7)

	; Make dividend positive if negative
	bit	7, 13(ix)
	jr	z, __moddi3_div_pos
	push	ix
	pop	hl
	ld	de, #6
	add	hl, de		; HL = &dividend
	call	__neg64_mem
__moddi3_div_pos:

	; Make divisor positive if negative
	bit	7, 21(ix)
	jr	z, __moddi3_dvs_pos
	push	ix
	pop	hl
	ld	de, #14
	add	hl, de		; HL = &divisor
	call	__neg64_mem
__moddi3_dvs_pos:

	call	__udiv64_setup
	call	__udiv64_core
	call	__udiv64_copy_rem

	; Apply remainder sign (follows dividend)
	pop	af
	bit	7, a
	jr	z, __moddi3_done
	; Negate result at sret pointer
	ld	l, 4(ix)
	ld	h, 5(ix)
	call	__neg64_mem
__moddi3_done:

	ld	sp, ix
	pop	ix
	ret			; caller cleanup (i64 return > 16 bits)
;===------------------------------------------------------------------------===;
;
; IEEE 754 f32 layout in HLDE:
;   H = SEEE EEEE  (S=sign, E=exponent[7:1])
;   L = EMMM MMMM  (E=exponent[0], M=mantissa[22:16])
;   D = MMMM MMMM  (M=mantissa[15:8])
;   E = MMMM MMMM  (M=mantissa[7:0])
;
; Exponent: bias 127. 0=zero/denorm, 255=inf/NaN.
; Mantissa: normalized numbers have implicit leading 1 (24-bit significand).
;
; Calling convention (__sdcccall(1)):
;   1st f32 arg → HLDE,  2nd f32 arg → stack
;   f32 return  → HLDE,  i32 return → HLDE,  i16 return → DE
;   IX callee-saved; A,BC,DE,HL caller-saved.
