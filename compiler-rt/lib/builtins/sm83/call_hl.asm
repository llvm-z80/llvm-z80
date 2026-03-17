; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl __call_hl

__call_hl:
	jp	(hl)

;===------------------------------------------------------------------------===;
;=== String / Memory Functions ==============================================;
;===------------------------------------------------------------------------===;
;
; All functions follow SM83 __sdcccall(1):
;   1st i16 arg in DE, 2nd i16 arg in BC, 3rd i16 arg on stack.
;   Return i16 in BC.
;
; SM83-specific instructions used:
;   LDI A,(HL) = LD A,(HL+) : load from HL, then HL++
;   LDD A,(HL) = LD A,(HL-) : load from HL, then HL--
;   LDI (HL),A = LD (HL+),A : store to HL, then HL++
;   LDHL SP,n  = LD HL,SP+n : HL = SP + signed_8bit_offset
;
; Stack argument access uses LDHL SP,n instead of IX+d (no IX on SM83).
;

;===------------------------------------------------------------------------===;
; _memcpy - Copy memory block
;
; Input:  DE = dest, BC = src, stack = size (i16)
; Output: BC = dest (original)
; Uses LDI A,(HL) for auto-incrementing source reads.
