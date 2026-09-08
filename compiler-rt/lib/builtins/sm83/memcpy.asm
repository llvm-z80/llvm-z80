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
; HL is the only register SM83 auto-increments, so the three rotate on entry to
; put the moving pointer there.
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
; SDCC lowers a struct assignment to __memcpy and keeps both names in one
; library module; defining only one of them pulls in SDCC's memcpy too.
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
