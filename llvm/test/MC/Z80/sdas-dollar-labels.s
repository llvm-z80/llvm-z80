; RUN: llvm-mc -triple=z80-unknown-sdcc -filetype=obj %s -o %t.o
; RUN: llvm-readelf -s %t.o | FileCheck %s
; RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS
; RUN: not llvm-mc -triple=z80 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ELF
; RUN: not llvm-mc -triple=z80-unknown-sdcc --defsym=ERR=1 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR

; CHECK-NOT: .L

.ifdef ERR
a:	nop
1$:	nop
; A normal label ends the scope of 1$.
; ERR: [[#@LINE+1]]:{{[0-9]+}}: error: dollar local label undefined
b:	jr	1$
.else
; ELF: [[#@LINE+2]]:{{[0-9]+}}: error: unexpected token at start of statement
; DIS: 0: 18 00
1$:	jr	2$
; DIS: 2: 18 fc
2$:	jr	1$
x = 5
; DIS: 4: 18 fa
	jr	1$
m:	nop
; DIS: 7: 18 fe
1$:	jr	00001$
.endif
