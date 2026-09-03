; RUN: not llvm-mc -triple=z80 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

; Fixups that resolve inside the assembler emit no relocation, so the
; linker's range checks never see them; the assembler must refuse the
; out-of-range values itself instead of silently truncating.

; CHECK: [[#@LINE+1]]:{{[0-9]+}}: error: fixup value out of range
	in	a, (late_port)
	ld	hl, late_imm16

.equ late_port, 0x1FF
.equ late_imm16, 0x5678
