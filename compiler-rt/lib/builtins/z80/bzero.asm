; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _bzero
	.globl _bzero_done

_bzero:
	push	hl		; save ptr
	ld	b, d		; BC = size
	ld	c, e
	ld	a, b
	or	c
	jr	z, _bzero_done
	ld	(hl), #0	; zero first byte
	dec	bc
	ld	a, b
	or	c
	jr	z, _bzero_done
	ld	d, h		; DE = HL (first byte)
	ld	e, l
	inc	de		; DE = HL + 1
	ldir			; propagate zero
_bzero_done:
	pop	de		; DE = original ptr
	ret

;===------------------------------------------------------------------------===;
;=== 32-bit Integer Arithmetic ===============================================;
;===------------------------------------------------------------------------===;
;
; 32-bit division and modulo via restoring division algorithm.
; Uses Z80 shadow registers (exx) for remainder tracking.
;
; Calling convention: __sdcccall(1)
;   First i32 argument in HLDE
;   Second i32 argument on stack (IX+4..IX+7 after frame setup)
;   Return i32 in HLDE
;   IX is callee-saved; A, BC, DE, HL are clobbered
;
