; RUN: llvm-mc -triple=sm83-unknown-sdcc -show-encoding %s | FileCheck %s
; RUN: llvm-mc -triple=z80-unknown-sdcc -show-encoding %s | FileCheck %s --check-prefix=Z80
; RUN: llvm-mc -triple=sm83 -show-encoding --defsym=ELF=1 %s | FileCheck %s --check-prefix=ELF

; sdasz80 and the ELF dialect read (hli) as memory at the symbol hli.

; CHECK: encoding: [0x2a]
; Z80: encoding: [0x3a,A,A]
; ELF: encoding: [0xfa,A,A]
	ld	a, (hli)
; CHECK: encoding: [0x32]
; Z80: encoding: [0x32,A,A]
; ELF: encoding: [0xea,A,A]
	ld	(hld), a
; CHECK: encoding: [0x2a]
	ld	a, (HLI)

.ifndef ELF
; (#hli) is memory at the symbol hli.
; CHECK: encoding: [0xfa,A,A]
	ld	a, (#hli)
.endif
