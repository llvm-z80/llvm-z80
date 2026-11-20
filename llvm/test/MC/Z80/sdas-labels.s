; RUN: llvm-mc -triple=z80-unknown-sdcc -filetype=obj %s -o %t.o
; RUN: llvm-readelf -s %t.o | FileCheck %s
; RUN: not llvm-mc -triple=z80 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ELF

; CHECK-DAG: GLOBAL DEFAULT {{.*}} g{{$}}
; CHECK-DAG: GLOBAL DEFAULT {{.*}} h{{$}}
; CHECK-DAG: LOCAL DEFAULT {{.*}} l{{$}}

; ELF: [[#@LINE+1]]:{{[0-9]+}}: error: unexpected token at start of statement
g::	nop
l:	nop
h::
	nop
