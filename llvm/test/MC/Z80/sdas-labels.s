; RUN: llvm-mc -triple=z80-unknown-sdcc -filetype=obj %s -o %t.o
; RUN: llvm-readelf -s %t.o | FileCheck %s
; RUN: not llvm-mc -triple=z80 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ELF
; RUN: not llvm-mc -triple=z80-unknown-sdcc --defsym=ERR=1 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR

; CHECK-DAG: GLOBAL DEFAULT {{.*}} g{{$}}
; CHECK-DAG: GLOBAL DEFAULT {{.*}} h{{$}}
; CHECK-DAG: LOCAL DEFAULT {{.*}} l{{$}}
; CHECK-DAG: 00000005 {{.*}} GLOBAL DEFAULT ABS k{{$}}
; CHECK-DAG: 00000003 {{.*}} GLOBAL DEFAULT {{[0-9]+}} G$m$0_0$0{{$}}

; ELF: [[#@LINE+1]]:{{[0-9]+}}: error: unexpected token at start of statement
g::	nop
l:	nop
h::
	nop
k == 5
G$m$0_0$0==.
	nop

.ifdef ERR
; ERR: [[#@LINE+1]]:{{[0-9]+}}: error: non-local symbol required
1$::	nop
; ERR: [[#@LINE+1]]:{{[0-9]+}}: error: non-local symbol required
1::	nop
; ERR: [[#@LINE+1]]:{{[0-9]+}}: error: non-local symbol required
.Lx::	nop
.endif
