; RUN: llvm-mc -triple=z80 -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=z80 -filetype=obj %s -o %t.o
; RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=OBJ
; RUN: not llvm-mc -triple=z80 --defsym=ERR=1 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR

; An 8-bit immediate takes -256..255 and 0xFF00..0xFFFF.

.ifdef ERR
; ERR: [[#@LINE+1]]:{{[0-9]+}}: error: immediate operand out of range
	ld	a, -257
; ERR: [[#@LINE+1]]:{{[0-9]+}}: error: immediate operand out of range
	ld	a, 256
; ERR: [[#@LINE+1]]:{{[0-9]+}}: error: immediate operand out of range
	ld	a, 0xFEFF
; ERR: [[#@LINE+1]]:{{[0-9]+}}: error: immediate operand out of range
	ld	a, 0x10000
.else
; CHECK: ld a,-256 ; encoding: [0x3e,0x00]
	ld	a, -256
; CHECK: and -129 ; encoding: [0xe6,0x7f]
	and	~0x80
; CHECK: ld (ix+1),-129 ; encoding: [0xdd,0x36,0x01,0x7f]
	ld	(ix+1), ~0x80
; CHECK: ld c,65386 ; encoding: [0x0e,0x6a]
	ld	c, 0xFF6A
; CHECK: ld a,65535 ; encoding: [0x3e,0xff]
	ld	a, 0xFFFF
; OBJ: 3e 38 ld a,$38
	ld	a, late
; OBJ: 3e 80 ld a,$80
	ld	a, late_high
.equ late, -200
.equ late_high, 0xFF80
.endif
