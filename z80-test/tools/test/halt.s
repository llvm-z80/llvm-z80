; RUN: llvm-mc -triple=z80 -filetype=obj %s -o %t.o
; RUN: ld.lld %t.o -o %t

; The result is read from DE on the Z80, and the cycles do not include HALT.
; RUN: z80-run --expect-result=0x000F --report=- %t | FileCheck %s --check-prefix=PASS
; PASS: {"cpu":"z80","cycles":10,"expected":15,"file":"{{.*}}","pc":{{[0-9]+}},"result":15,"stop":"halt"}

; RUN: not z80-run %t 2>&1 | FileCheck %s --check-prefix=MISMATCH
; MISMATCH: z80-run: {{.*}}: result 0x000F, expected 0x0000

; RUN: z80-run --expect-result=any -v %t 2>&1 | FileCheck %s --check-prefix=VERBOSE
; VERBOSE: z80-run: {{.*}}: halted at 0x{{[0-9A-F]+}} after 10 cycles with 0x000F

; Another register, or the symbol of a 16-bit variable.
; RUN: z80-run --result=hl %t
; RUN: z80-run --result=value --expect-result=0x1234 %t
; RUN: z80-run --result=value --expect-result=4660 %t

	.globl	_start
_start:
	ld	de, 0x000F
	halt

	.data
	.globl	value
value:
	.short	0x1234
