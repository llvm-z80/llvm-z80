; RUN: not llvm-mc -triple=z80 %s -o /dev/null 2>&1 | FileCheck %s
; RUN: not llvm-mc -triple=sm83 %s -o /dev/null 2>&1 | FileCheck %s

; Where an instruction takes an address or a branch target, another kind of
; operand is an error, not a line left out of the output.

; CHECK: [[#@LINE+1]]:{{[0-9]+}}: error:
	ld	bc, 2 (ix)
; CHECK: [[#@LINE+1]]:{{[0-9]+}}: error:
	ld	hl, (ix+2)
; CHECK: [[#@LINE+1]]:{{[0-9]+}}: error:
	ld	sp, 2 (ix)
; CHECK: [[#@LINE+1]]:{{[0-9]+}}: error: invalid operand for instruction
	jr	(hl)
; CHECK: [[#@LINE+1]]:{{[0-9]+}}: error: invalid operand for instruction
	jr	nz, (ix+2)
; CHECK: [[#@LINE+1]]:{{[0-9]+}}: error: invalid operand for instruction
	djnz	(ix)
