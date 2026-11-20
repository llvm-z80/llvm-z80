; RUN: llvm-mc -triple=z80-unknown-sdcc -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=z80-unknown-sdcc -filetype=obj %s -o %t.o
; RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
; RUN: not llvm-mc -triple=z80-unknown-sdcc --defsym=ERR=1 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR
; RUN: not llvm-mc -triple=z80 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ELF

.ifdef ERR
; sdas takes the high byte of ext+1 here, not (>ext)+1.
; ERR: [[#@LINE+1]]:{{[0-9]+}}: error: & 0xff and >> 8 must apply to the whole expression
	ld	a, #>ext+1
.else
; ELF: [[#@LINE+2]]:{{[0-9]+}}: error: unknown token in expression
; CHECK: ld a,#52 ; encoding: [0x3e,0x34]
	ld	a, #<0x1234
; CHECK: ld a,#19 ; encoding: [0x3e,0x13]
	ld	a, #>0x1200+1
	ld	a, #<(ext+1)
	ld	b, #>ext
	.db	<ext, >ext
.endif

; RELOC: 0x5 R_Z80_BYTE0 ext 0x1
; RELOC: 0x7 R_Z80_BYTE1 ext 0x0
; RELOC: 0x8 R_Z80_BYTE0 ext 0x0
; RELOC: 0x9 R_Z80_BYTE1 ext 0x0
