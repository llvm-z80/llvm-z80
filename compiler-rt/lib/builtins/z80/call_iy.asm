; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl __call_iy

;===------------------------------------------------------------------------===;
; __call_iy - Indirect call trampoline
;
; Z80 has no CALL (reg) instruction. This trampoline enables indirect calls
; by jumping to the address in IY. The caller uses CALL __call_iy, which
; pushes the return address, then JP (IY) transfers control to the target.
; When the target function RETurns, it returns to the caller's call site.
;===------------------------------------------------------------------------===;
__call_iy:
	jp	(iy)
