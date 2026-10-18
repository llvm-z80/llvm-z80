; RUN: llvm-mc -triple=sm83 -filetype=obj %s -o %t.o
; RUN: ld.lld %t.o -o %t

; The CPU comes from the ELF flags, and the result from BC on the SM83.
; RUN: z80-run --expect-result=7 --report=- %t | FileCheck %s
; CHECK: {"cpu":"sm83",
; CHECK-SAME: "result":7,"stop":"halt"}

; RUN: not z80-run --cpu=z80 --report=- %t 2>&1 | FileCheck %s --check-prefix=MISMATCH
; MISMATCH: z80-run: error: {{.*}} is built for sm83, not z80
; MISMATCH-NEXT: {"error":"cpu-mismatch",

	.globl	_start
_start:
	ld	bc, 7
	halt
