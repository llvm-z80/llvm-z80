; RUN: llvm-mc -triple=z80 -show-encoding %s | FileCheck %s
; RUN: echo '0x76 0xdd 0x76 0xfd 0x76' | llvm-mc -triple=z80 --disassemble -show-encoding | FileCheck %s --check-prefix=DIS
;
; HALT behind a DD or FD prefix still disassembles, but the assembler only
; produces the one-byte HALT.

; CHECK: halt ; encoding: [0x76]
	halt

; DIS:      halt ; encoding: [0x76]
; DIS-NEXT: halt ; encoding: [0xdd,0x76]
; DIS-NEXT: halt ; encoding: [0xfd,0x76]
