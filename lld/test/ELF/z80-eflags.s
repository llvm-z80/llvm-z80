; REQUIRES: z80
; RUN: rm -rf %t && split-file %s %t && cd %t
; RUN: llvm-mc -filetype=obj -triple=z80 nop.s -o z80.o
; RUN: llvm-mc -filetype=obj -triple=sm83 nop.s -o sm83.o
; RUN: yaml2obj none.yaml -o none.o

; The output takes the CPU of its inputs. An object without a CPU value counts
; as Z80.

; RUN: ld.lld sm83.o -o out && llvm-readelf -h out | FileCheck %s --check-prefix=SM83
; SM83: Flags: 0x5, SM83{{$}}

; A link that extracts nothing from its archives has no object, and the output
; is Z80.

; RUN: llvm-ar rcs sm83.a sm83.o
; RUN: ld.lld sm83.a -o out && llvm-readelf -h out | FileCheck %s --check-prefix=Z80
; Z80: Flags: 0x1, Z80{{$}}

; RUN: not ld.lld z80.o sm83.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR-SM83
; RUN: not ld.lld none.o sm83.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR-NONE
; ERR-SM83: error: sm83.o: cannot link SM83 code with Z80 code from z80.o
; ERR-NONE: error: sm83.o: cannot link SM83 code with Z80 code from none.o

#--- nop.s
	nop

#--- none.yaml
--- !ELF
FileHeader:
  Class:   ELFCLASS32
  Data:    ELFDATA2LSB
  Type:    ET_REL
  Machine: 0xDC
