; RUN: llvm-mc -triple=z80-unknown-sdcc -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=sm83-unknown-sdcc -show-encoding --defsym=SM83=1 %s | FileCheck %s --check-prefix=SM83
; RUN: not llvm-mc -triple=z80 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ELF

.ifdef SM83
; SM83: encoding: [0xf0,0x80]
	ldh	a, (#0x80)
; SM83: encoding: [0xea,0x00,0xc0]
	ld	(#0xc000), a
.else
; ELF: [[#@LINE+2]]:{{[0-9]+}}: error: unknown token in expression
; CHECK: encoding: [0x3a,0x34,0x12]
	ld	a, (#0x1234)
; CHECK: encoding: [0x22,0x00,0xc0]
	ld	(#0xc000), hl
; CHECK: encoding: [0xdb,0x05]
	in	a, (#5)
; CHECK: encoding: [0xd3,0x05]
	out	(#5), a
; (#hl) is memory at the symbol hl.
; CHECK: encoding: [0x3a,A,A]
	ld	a, (#hl)
.endif
