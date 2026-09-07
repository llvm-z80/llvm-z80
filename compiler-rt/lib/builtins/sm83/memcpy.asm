; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memcpy
	.globl ___memcpy
	.globl ___z80_memcpy_builtin

;===------------------------------------------------------------------------===;
; ___z80_memcpy_builtin - Copy memory block (CallingConv::Z80_Builtin)
;
; Input:  DE = dest, BC = src, HL = size
; Output: none
;
; The convention follows __sdcccall(1)'s order for this subtarget and adds the
; pair that convention does not reach, so the first two arguments arrive where
; the C entry below already had them and only the size is new.  The loop needs
; its moving pointer in HL, the only register SM83 auto-increments, so the
; three rotate once on entry.
;===------------------------------------------------------------------------===;
___z80_memcpy_builtin:
	ld	a, h
	or	l
	ret	z		; size == 0
	push	bc		; hold src
	ld	b, h
	ld	c, l		; BC = size
	pop	hl		; HL = src
___z80_memcpy_loop:
	ld	a, (hl+)
	ld	(de), a
	inc	de
	dec	bc
	ld	a, b
	or	c
	jr	nz, ___z80_memcpy_loop
	ret

;===------------------------------------------------------------------------===;
; _memcpy - Copy memory block, C entry point
;
; Input:  DE = dest, BC = src, stack = size (i16)
; Output: BC = dest (original)
;
; SDCC lowers a struct assignment to a call to __memcpy, its own name for this
; routine, and keeps both names in one library module.  Defining both here
; keeps that module, and the duplicate memcpy it carries, out of the link.
;===------------------------------------------------------------------------===;
_memcpy:
___memcpy:
	push	de		; save dest for return value
	ldhl	sp, #4		; [saved DE(2), ret addr(2), size]
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = size; DE and BC are already in place
	call	___z80_memcpy_builtin
	pop	bc		; BC = original dest (return value)
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip 2 bytes of stack args
	jp	(hl)
