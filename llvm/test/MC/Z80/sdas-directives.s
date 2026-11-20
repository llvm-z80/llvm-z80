; RUN: llvm-mc -triple=z80-unknown-sdcc -filetype=obj %s -o %t.o
; RUN: llvm-objdump -s -j .text %t.o | FileCheck %s
; RUN: not llvm-mc -triple=z80 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ELF

; ELF: [[#@LINE+1]]:{{[0-9]+}}: error: unknown directive
	.module	m
	.optsdcc	-mz80 sdcccall(1)
	.title	"t"
	.sbttl	s
$1:	nop
	jr	$1
	.db	1, #2
	.fcb	3
	.dw	0x1234, #0x5678
	.word	0x9abc
	.fdb	0xdef0

; CHECK: 0000 0018fd01 02033412 7856bc9a f0de
