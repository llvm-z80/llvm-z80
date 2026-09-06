; RUN: not llvm-mc -triple=sm83 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

; An LDH operand outside the high page would keep only its low byte and
; quietly address a different hardware register.

; CHECK: [[#@LINE+1]]:{{[0-9]+}}: error: LDH address must be in the range 0xFF00 to 0xFFFF
	ldh	a, (late_vram)

; A real high-page register still assembles.
	ldh	a, (late_ly)

.equ late_vram, 0x8000
.equ late_ly, 0xFF44
