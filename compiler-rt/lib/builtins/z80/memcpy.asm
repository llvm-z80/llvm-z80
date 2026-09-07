; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _memcpy
	.globl ___memcpy
	.globl ___z80_memcpy_builtin

;===------------------------------------------------------------------------===;
; ___z80_memcpy_builtin - Copy memory block (CallingConv::Z80_Builtin)
;
; Input:  HL = dest, DE = src, BC = size
; Output: none
;
; The form the compiler calls for a copy it synthesised itself.  Taking the
; size in BC saves the IX frame, the stack read, the callee cleanup and the
; destination this would otherwise preserve to return.
;
; LDIR copies (HL)->(DE) and steps both up, so the pointers arrive swapped
; relative to the C argument order.  It decrements BC before testing it, so a
; zero size has to be turned away first or it would copy 65536 bytes.
;===------------------------------------------------------------------------===;
___z80_memcpy_builtin:
	ex	de, hl		; HL = src, DE = dest (LDIR format)
	ld	a, b
	or	c
	ret	z		; size == 0
	ldir
	ret

;===------------------------------------------------------------------------===;
; _memcpy - Copy memory block, C entry point
;
; Input:  HL = dest, DE = src, stack = size (i16)
; Output: DE = dest (original)
;
; SDCC lowers a struct assignment to a call to __memcpy, its own name for this
; routine.  Both names live in one module in SDCC's library, so a program that
; links this runtime and reaches for __memcpy would otherwise drag in SDCC's
; memcpy alongside ours and fail on the duplicate.  Defining both here keeps
; that module out of the link entirely.
;===------------------------------------------------------------------------===;
_memcpy:
___memcpy:
	push	ix
	ld	ix, #0
	add	ix, sp
	ld	c, 4(ix)	; BC = size (3rd arg from stack)
	ld	b, 5(ix)
	push	hl		; save dest for return value
	call	___z80_memcpy_builtin
	pop	de		; DE = original dest (return value)
	pop	ix
	pop	bc		; save return address
	inc	sp
	inc	sp		; callee-cleanup: skip 2 bytes of stack args
	push	bc		; re-push return address
	ret
