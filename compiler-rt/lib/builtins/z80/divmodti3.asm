; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl __udiv128_setup
	.globl __udiv128_core
	.globl __udiv128_loop
	.globl __udiv128_skip
	.globl __udiv128_copy_quot
	.globl __udiv128_copy_rem
	.globl __neg128_mem
	.globl ___udivti3
	.globl ___divti3
	.globl __divti3_div_pos
	.globl __divti3_dvs_pos
	.globl __divti3_done
	.globl ___umodti3
	.globl ___modti3
	.globl __modti3_div_pos
	.globl __modti3_dvs_pos
	.globl __modti3_done

;===------------------------------------------------------------------------===;
; 128-bit Integer Division and Modulo for Z80
;
; Restoring division algorithm, memory-based.
;
; Calling convention (sret demotion, SDCC __sdcccall(1)):
;   Stack (caller pushes): sret pointer (2B), dividend (16B), divisor (16B)
;   sret pointer is at lowest address (pushed last by caller)
;
; Stack frame after push ix; ld ix,#0; add ix,sp; allocate 32:
;   IX-32..IX-17 = remainder (16 bytes, little-endian, IX-32 = LSB)
;   IX-16..IX-1  = quotient  (16 bytes, little-endian, IX-16 = LSB)
;   IX+0,1       = saved IX
;   IX+2,3       = return address
;   IX+4,5       = sret pointer (caller pushed on stack)
;   IX+6..IX+21  = dividend (16 bytes, little-endian, IX+6  = LSB)
;   IX+22..IX+37 = divisor  (16 bytes, little-endian, IX+22 = LSB)
;
;===------------------------------------------------------------------------===;

;===------------------------------------------------------------------------===;
; __neg128_mem - Negate 16-byte value in memory (two's complement)
; Input: HL = pointer to 16-byte little-endian value
; Clobbers: A, DE, HL
;===------------------------------------------------------------------------===;
__neg128_mem:
	; Complement all 16 bytes
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
	inc	hl
	ld	a, (hl)
	cpl
	ld	(hl), a
	; HL now points to byte 15; rewind to byte 0
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
; __udiv128_setup - Copy dividend to quotient, zero remainder
;===------------------------------------------------------------------------===;
__udiv128_setup:
	; Copy dividend (IX+6..IX+21) to quotient (IX-16..IX-1)
	ld	a, 6(ix)
	ld	-16(ix), a
	ld	a, 7(ix)
	ld	-15(ix), a
	ld	a, 8(ix)
	ld	-14(ix), a
	ld	a, 9(ix)
	ld	-13(ix), a
	ld	a, 10(ix)
	ld	-12(ix), a
	ld	a, 11(ix)
	ld	-11(ix), a
	ld	a, 12(ix)
	ld	-10(ix), a
	ld	a, 13(ix)
	ld	-9(ix), a
	ld	a, 14(ix)
	ld	-8(ix), a
	ld	a, 15(ix)
	ld	-7(ix), a
	ld	a, 16(ix)
	ld	-6(ix), a
	ld	a, 17(ix)
	ld	-5(ix), a
	ld	a, 18(ix)
	ld	-4(ix), a
	ld	a, 19(ix)
	ld	-3(ix), a
	ld	a, 20(ix)
	ld	-2(ix), a
	ld	a, 21(ix)
	ld	-1(ix), a

	; Zero remainder (IX-32..IX-17)
	xor	a
	ld	-32(ix), a
	ld	-31(ix), a
	ld	-30(ix), a
	ld	-29(ix), a
	ld	-28(ix), a
	ld	-27(ix), a
	ld	-26(ix), a
	ld	-25(ix), a
	ld	-24(ix), a
	ld	-23(ix), a
	ld	-22(ix), a
	ld	-21(ix), a
	ld	-20(ix), a
	ld	-19(ix), a
	ld	-18(ix), a
	ld	-17(ix), a

	ret

;===------------------------------------------------------------------------===;
; __udiv128_core - Core 128-bit unsigned division (restoring algorithm)
;
; Performs 128 iterations of shift-compare-subtract.
; Uses two nested loops: outer (C) counts groups of 64, inner (B) counts 64.
;===------------------------------------------------------------------------===;
__udiv128_core:
	ld	c, #2		; outer loop: 2 groups of 64

__udiv128_core_outer:
	ld	b, #64		; inner loop: 64 iterations

__udiv128_loop:
	; Step 1: Shift quotient left by 1 (16 bytes, LSB at IX-16)
	sla	-16(ix)
	rl	-15(ix)
	rl	-14(ix)
	rl	-13(ix)
	rl	-12(ix)
	rl	-11(ix)
	rl	-10(ix)
	rl	-9(ix)
	rl	-8(ix)
	rl	-7(ix)
	rl	-6(ix)
	rl	-5(ix)
	rl	-4(ix)
	rl	-3(ix)
	rl	-2(ix)
	rl	-1(ix)

	; Step 2: Shift carry into remainder (16 bytes, LSB at IX-32)
	rl	-32(ix)
	rl	-31(ix)
	rl	-30(ix)
	rl	-29(ix)
	rl	-28(ix)
	rl	-27(ix)
	rl	-26(ix)
	rl	-25(ix)
	rl	-24(ix)
	rl	-23(ix)
	rl	-22(ix)
	rl	-21(ix)
	rl	-20(ix)
	rl	-19(ix)
	rl	-18(ix)
	rl	-17(ix)

	; Step 3: Compare remainder >= divisor (non-destructive)
	ld	a, -32(ix)
	sub	22(ix)
	ld	a, -31(ix)
	sbc	a, 23(ix)
	ld	a, -30(ix)
	sbc	a, 24(ix)
	ld	a, -29(ix)
	sbc	a, 25(ix)
	ld	a, -28(ix)
	sbc	a, 26(ix)
	ld	a, -27(ix)
	sbc	a, 27(ix)
	ld	a, -26(ix)
	sbc	a, 28(ix)
	ld	a, -25(ix)
	sbc	a, 29(ix)
	ld	a, -24(ix)
	sbc	a, 30(ix)
	ld	a, -23(ix)
	sbc	a, 31(ix)
	ld	a, -22(ix)
	sbc	a, 32(ix)
	ld	a, -21(ix)
	sbc	a, 33(ix)
	ld	a, -20(ix)
	sbc	a, 34(ix)
	ld	a, -19(ix)
	sbc	a, 35(ix)
	ld	a, -18(ix)
	sbc	a, 36(ix)
	ld	a, -17(ix)
	sbc	a, 37(ix)
	jp	c, __udiv128_skip	; remainder < divisor

	; Step 4: remainder >= divisor: subtract
	ld	a, -32(ix)
	sub	22(ix)
	ld	-32(ix), a
	ld	a, -31(ix)
	sbc	a, 23(ix)
	ld	-31(ix), a
	ld	a, -30(ix)
	sbc	a, 24(ix)
	ld	-30(ix), a
	ld	a, -29(ix)
	sbc	a, 25(ix)
	ld	-29(ix), a
	ld	a, -28(ix)
	sbc	a, 26(ix)
	ld	-28(ix), a
	ld	a, -27(ix)
	sbc	a, 27(ix)
	ld	-27(ix), a
	ld	a, -26(ix)
	sbc	a, 28(ix)
	ld	-26(ix), a
	ld	a, -25(ix)
	sbc	a, 29(ix)
	ld	-25(ix), a
	ld	a, -24(ix)
	sbc	a, 30(ix)
	ld	-24(ix), a
	ld	a, -23(ix)
	sbc	a, 31(ix)
	ld	-23(ix), a
	ld	a, -22(ix)
	sbc	a, 32(ix)
	ld	-22(ix), a
	ld	a, -21(ix)
	sbc	a, 33(ix)
	ld	-21(ix), a
	ld	a, -20(ix)
	sbc	a, 34(ix)
	ld	-20(ix), a
	ld	a, -19(ix)
	sbc	a, 35(ix)
	ld	-19(ix), a
	ld	a, -18(ix)
	sbc	a, 36(ix)
	ld	-18(ix), a
	ld	a, -17(ix)
	sbc	a, 37(ix)
	ld	-17(ix), a

	; Set quotient bit 0
	set	0, -16(ix)

__udiv128_skip:
	dec	b
	jp	nz, __udiv128_loop
	dec	c
	jp	nz, __udiv128_core_outer
	ret

;===------------------------------------------------------------------------===;
; __udiv128_copy_quot - Copy quotient to sret pointer
; Copies IX-16..IX-1 to address at IX+4,5
;===------------------------------------------------------------------------===;
__udiv128_copy_quot:
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
	inc	hl
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
; __udiv128_copy_rem - Copy remainder to sret pointer
; Copies IX-32..IX-17 to address at IX+4,5
;===------------------------------------------------------------------------===;
__udiv128_copy_rem:
	ld	l, 4(ix)
	ld	h, 5(ix)
	ld	a, -32(ix)
	ld	(hl), a
	inc	hl
	ld	a, -31(ix)
	ld	(hl), a
	inc	hl
	ld	a, -30(ix)
	ld	(hl), a
	inc	hl
	ld	a, -29(ix)
	ld	(hl), a
	inc	hl
	ld	a, -28(ix)
	ld	(hl), a
	inc	hl
	ld	a, -27(ix)
	ld	(hl), a
	inc	hl
	ld	a, -26(ix)
	ld	(hl), a
	inc	hl
	ld	a, -25(ix)
	ld	(hl), a
	inc	hl
	ld	a, -24(ix)
	ld	(hl), a
	inc	hl
	ld	a, -23(ix)
	ld	(hl), a
	inc	hl
	ld	a, -22(ix)
	ld	(hl), a
	inc	hl
	ld	a, -21(ix)
	ld	(hl), a
	inc	hl
	ld	a, -20(ix)
	ld	(hl), a
	inc	hl
	ld	a, -19(ix)
	ld	(hl), a
	inc	hl
	ld	a, -18(ix)
	ld	(hl), a
	inc	hl
	ld	a, -17(ix)
	ld	(hl), a
	ret

;===------------------------------------------------------------------------===;
; ___udivti3 - Unsigned 128-bit division
;
; Input:  stack = sret pointer (2B) + dividend (16B) + divisor (16B)
; Output: writes 16-byte quotient to sret pointer
;===------------------------------------------------------------------------===;
___udivti3:
	push	ix
	ld	ix, #0
	add	ix, sp
	; Allocate 32 bytes for quotient(16) + remainder(16)
	ld	hl, #-32
	add	hl, sp
	ld	sp, hl

	call	__udiv128_setup
	call	__udiv128_core
	call	__udiv128_copy_quot

	ld	sp, ix
	pop	ix
	ret

;===------------------------------------------------------------------------===;
; ___divti3 - Signed 128-bit division
;
; Input:  stack = sret pointer (2B) + dividend (16B) + divisor (16B)
; Output: writes 16-byte quotient to sret pointer
;===------------------------------------------------------------------------===;
___divti3:
	push	ix
	ld	ix, #0
	add	ix, sp
	; Allocate 32 bytes for quotient(16) + remainder(16)
	ld	hl, #-32
	add	hl, sp
	ld	sp, hl

	; Result sign = XOR of input signs
	ld	a, 21(ix)	; MSB of dividend (sign byte)
	xor	37(ix)		; XOR with MSB of divisor
	push	af		; save result sign (bit 7)

	; Make dividend positive if negative
	bit	7, 21(ix)
	jr	z, __divti3_div_pos
	push	ix
	pop	hl
	ld	de, #6
	add	hl, de		; HL = &dividend
	call	__neg128_mem
__divti3_div_pos:

	; Make divisor positive if negative
	bit	7, 37(ix)
	jr	z, __divti3_dvs_pos
	push	ix
	pop	hl
	ld	de, #22
	add	hl, de		; HL = &divisor
	call	__neg128_mem
__divti3_dvs_pos:

	call	__udiv128_setup
	call	__udiv128_core
	call	__udiv128_copy_quot

	; Apply result sign
	pop	af
	bit	7, a
	jr	z, __divti3_done
	; Negate result at sret pointer
	ld	l, 4(ix)
	ld	h, 5(ix)
	call	__neg128_mem
__divti3_done:

	ld	sp, ix
	pop	ix
	ret

;===------------------------------------------------------------------------===;
; ___umodti3 - Unsigned 128-bit modulo
;
; Input:  stack = sret pointer (2B) + dividend (16B) + divisor (16B)
; Output: writes 16-byte remainder to sret pointer
;===------------------------------------------------------------------------===;
___umodti3:
	push	ix
	ld	ix, #0
	add	ix, sp
	; Allocate 32 bytes for quotient(16) + remainder(16)
	ld	hl, #-32
	add	hl, sp
	ld	sp, hl

	call	__udiv128_setup
	call	__udiv128_core
	call	__udiv128_copy_rem

	ld	sp, ix
	pop	ix
	ret

;===------------------------------------------------------------------------===;
; ___modti3 - Signed 128-bit modulo
;
; Input:  stack = sret pointer (2B) + dividend (16B) + divisor (16B)
; Output: writes 16-byte remainder to sret pointer (sign follows dividend)
;===------------------------------------------------------------------------===;
___modti3:
	push	ix
	ld	ix, #0
	add	ix, sp
	; Allocate 32 bytes for quotient(16) + remainder(16)
	ld	hl, #-32
	add	hl, sp
	ld	sp, hl

	; Remainder sign = dividend sign
	ld	a, 21(ix)	; MSB of dividend (sign byte)
	push	af

	; Make dividend positive if negative
	bit	7, 21(ix)
	jr	z, __modti3_div_pos
	push	ix
	pop	hl
	ld	de, #6
	add	hl, de		; HL = &dividend
	call	__neg128_mem
__modti3_div_pos:

	; Make divisor positive if negative
	bit	7, 37(ix)
	jr	z, __modti3_dvs_pos
	push	ix
	pop	hl
	ld	de, #22
	add	hl, de		; HL = &divisor
	call	__neg128_mem
__modti3_dvs_pos:

	call	__udiv128_setup
	call	__udiv128_core
	call	__udiv128_copy_rem

	; Apply dividend sign to remainder
	pop	af
	bit	7, a
	jr	z, __modti3_done
	; Negate result at sret pointer
	ld	l, 4(ix)
	ld	h, 5(ix)
	call	__neg128_mem
__modti3_done:

	ld	sp, ix
	pop	ix
	ret
