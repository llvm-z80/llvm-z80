; RUN: llvm-mc -triple=z80 -filetype=obj %s -o - | llvm-readelf -h - | FileCheck %s --check-prefix=Z80
; RUN: llvm-mc -triple=z80 -mcpu=z80-undoc -filetype=obj %s -o - | llvm-readelf -h - | FileCheck %s --check-prefix=Z80
; RUN: llvm-mc -triple=z80 -mcpu=z180 -filetype=obj %s -o - | llvm-readelf -h - | FileCheck %s --check-prefix=Z180
; RUN: llvm-mc -triple=z80 -mcpu=r800 -filetype=obj %s -o - | llvm-readelf -h - | FileCheck %s --check-prefix=R800
; RUN: llvm-mc -triple=z80 -mcpu=ez80 -filetype=obj %s -o - | llvm-readelf -h - | FileCheck %s --check-prefix=EZ80
; RUN: llvm-mc -triple=sm83 -filetype=obj %s -o - | llvm-readelf -h - | FileCheck %s --check-prefix=SM83
; RUN: llvm-mc -triple=z80 -mcpu=gbz80 -filetype=obj %s -o - | llvm-readelf -h - | FileCheck %s --check-prefix=SM83
; RUN: llvm-mc -triple=sm83 -filetype=obj %s -o - | llvm-readobj -h - | FileCheck %s --check-prefix=SM83-LLVM

; The machine number and the CPU values in e_flags are the ones GNU binutils
; uses for Z80 ELF files.

; Z80:  Machine: Zilog Z80
; Z80:  Flags: 0x1, Z80{{$}}
; Z180: Flags: 0x2, Z180{{$}}
; R800: Flags: 0x3, R800{{$}}
; EZ80: Flags: 0x4, EZ80{{$}}
; SM83: Machine: Zilog Z80
; SM83: Flags: 0x5, SM83{{$}}

; SM83-LLVM:      Machine: EM_Z80 (0xDC)
; SM83-LLVM:      Flags [ (0x5)
; SM83-LLVM-NEXT:   EF_Z80_MACH_SM83 (0x5)
; SM83-LLVM-NEXT: ]

	nop
