; REQUIRES: z80
; RUN: rm -rf %t && split-file %s %t && cd %t
; RUN: llvm-mc -filetype=obj -triple=z80 nop.s -o z80.o
; RUN: llvm-mc -filetype=obj -triple=z80 -mcpu=z180 nop.s -o z180.o
; RUN: llvm-mc -filetype=obj -triple=z80 -mcpu=r800 nop.s -o r800.o
; RUN: llvm-mc -filetype=obj -triple=z80 -mcpu=ez80 nop.s -o ez80.o
; RUN: llvm-mc -filetype=obj -triple=sm83 nop.s -o sm83.o
; RUN: yaml2obj none.yaml -o none.o

; The output takes the most capable input CPU, under the binutils rules for
; which CPUs mix. An object without a CPU value counts as Z80.

; RUN: ld.lld z80.o z180.o -o out && llvm-readelf -h out | FileCheck %s --check-prefix=Z180
; RUN: ld.lld none.o z180.o -o out && llvm-readelf -h out | FileCheck %s --check-prefix=Z180
; RUN: ld.lld z180.o ez80.o z80.o -o out && llvm-readelf -h out | FileCheck %s --check-prefix=EZ80
; RUN: ld.lld sm83.o -o out && llvm-readelf -h out | FileCheck %s --check-prefix=SM83
; Z180: Flags: 0x2, Z180{{$}}
; EZ80: Flags: 0x4, EZ80{{$}}
; SM83: Flags: 0x5, SM83{{$}}

; RUN: not ld.lld z80.o sm83.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR-SM83
; RUN: not ld.lld none.o sm83.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR-NONE
; RUN: not ld.lld z180.o r800.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR-R800
; ERR-SM83: error: sm83.o: cannot link SM83 code with Z80 code from z80.o
; ERR-NONE: error: sm83.o: cannot link SM83 code with Z80 code from none.o
; ERR-R800: error: r800.o: cannot link R800 code with Z180 code from z180.o

#--- nop.s
	nop

#--- none.yaml
--- !ELF
FileHeader:
  Class:   ELFCLASS32
  Data:    ELFDATA2LSB
  Type:    ET_REL
  Machine: 0xDC
