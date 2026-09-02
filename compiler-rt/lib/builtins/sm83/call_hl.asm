; SPDX-License-Identifier: Zlib OR Apache-2.0 WITH LLVM-exception OR MIT
	.area _CODE
	.globl __call_hl

;===------------------------------------------------------------------------===;
; __call_hl - Indirect call trampoline
;
; SM83 has no CALL (reg) instruction. This trampoline enables indirect calls
; by jumping to the address in HL. The caller uses CALL __call_hl, which
; pushes the return address, then JP (HL) transfers control to the target.
; When the target function RETurns, it returns to the caller's call site.
;===------------------------------------------------------------------------===;

__call_hl:
	jp	(hl)
