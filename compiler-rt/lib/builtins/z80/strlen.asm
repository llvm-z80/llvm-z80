; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl _strlen
	.globl _strlen_loop

;===------------------------------------------------------------------------===;
; _strlen - Get string length
;
; Input:  HL = pointer to null-terminated string
; Output: DE = length (number of bytes before null terminator)
;===------------------------------------------------------------------------===;
_strlen:
	ld	de, #0		; length = 0
_strlen_loop:
	ld	a, (hl)
	or	a
	ret	z		; found null terminator, DE = length
	inc	hl
	inc	de
	jr	_strlen_loop

;===------------------------------------------------------------------------===;
; _strnlen - Bounded string length
;
; Input:  HL = string, DE = maxlen
