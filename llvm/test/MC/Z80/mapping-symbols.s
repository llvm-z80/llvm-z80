; RUN: llvm-mc -triple=z80 -filetype=obj %s -o - | llvm-objdump -t - | FileCheck %s

; $x and $d are written only in executable sections, and each section keeps
; its own state across .section and .popsection.

	.text
	nop
	.pushsection .text.c,"ax"
	.byte	1
	.popsection
	.byte	2
	.section .data,"aw"
	.byte	3
	.text
	nop
	.zero	2
	.section .text.c,"ax"
	.byte	4
	nop

; CHECK:      SYMBOL TABLE:
; CHECK-NEXT: 00000000 l .text 00000000 $x
; CHECK-NEXT: 00000000 l .text.c 00000000 $d
; CHECK-NEXT: 00000001 l .text 00000000 $d
; CHECK-NEXT: 00000002 l .text 00000000 $x
; CHECK-NEXT: 00000003 l .text 00000000 $d
; CHECK-NEXT: 00000002 l .text.c 00000000 $x
; CHECK-NOT:  .data
