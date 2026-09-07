; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memset
	.globl ___z80_memset_builtin

;===------------------------------------------------------------------------===;
; ___z80_memset_builtin - Fill memory block (CallingConv::Z80_Builtin)
;
; Input:  HL = dest, DE = value (E = byte), BC = size
; Output: none
;
; Writes the first byte, then uses LDIR to propagate it across the rest.
;===------------------------------------------------------------------------===;
___z80_memset_builtin:
	ld	a, b
	or	c
	ret	z		; size == 0
	ld	(hl), e		; write first byte
	dec	bc		; remaining = size - 1
	ld	a, b
	or	c
	ret	z		; size was 1
	ld	d, h		; DE = HL (points to first byte)
	ld	e, l
	inc	de		; DE = HL + 1 (next byte)
	ldir			; copy first byte to remaining
	ret

;===------------------------------------------------------------------------===;
; _memset - Fill memory block, C entry point
;
; Input:  HL = dest, DE = value (E = byte), stack = size (i16)
; Output: DE = dest (original)
;===------------------------------------------------------------------------===;
_memset:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, 4(ix)	; BC = size
	ld	b, 5(ix)
	push	hl		; save dest for return value
	call	___z80_memset_builtin
	pop	de		; DE = original dest (return value)
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc		; re-push return address
	ret
