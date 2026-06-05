; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memset
	.globl _memset_loop
	.globl _memset_done

_memset:
	push	de		; save dest for return
	; Load size from stack
	ldhl	sp, #4
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a		; HL = size
	ld	a, h
	or	l
	jr	z, _memset_done
	; Rearrange: HL=dest, BC=size, E=fill byte
	ld	a, c		; A = fill byte
	ld	b, h
	ld	c, l		; BC = size
	ld	h, d
	ld	l, e		; HL = dest
	ld	e, a		; E = fill byte (saved for reload)
_memset_loop:
	ld	a, e		; A = fill byte
	ld	(hl+), a		; *(HL++) = fill
	dec	bc
	ld	a, b
	or	c
	jr	nz, _memset_loop
_memset_done:
	pop	bc		; BC = original dest (return value)
	pop	hl		; return address
	add	sp, #2		; callee-cleanup: skip 2 bytes of stack args
	jp	(hl)

;===------------------------------------------------------------------------===;
; _strlen - Get string length
;
; Input:  DE = pointer to null-terminated string
; Output: BC = length (number of bytes before null terminator)
; Uses LDI A,(HL) for auto-incrementing string scan.
