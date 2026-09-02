; RUN: llc -mtriple=z80 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=z80 -verify-machineinstrs -z80-asm-format=sdasz80 < %s | FileCheck %s --check-prefix=SDCC
; RUN: llc -mtriple=z80 -verify-machineinstrs -filetype=obj < %s -o %t.o
; RUN: llvm-readelf -r %t.o | FileCheck %s --check-prefix=RELOC

; A byte of a symbol's link-time address loads as an 8-bit immediate with a
; low/high-byte fixup instead of materializing the full 16-bit address.
; The GB banking convention encodes bank numbers as symbol addresses, so
; this is the codegen for gb-bank's Group::bank().

@__bank_music = external global i8

; CHECK-LABEL: get_bank:
; CHECK: ld a,z80_16lo(___bank_music)
; CHECK-NOT: ld hl
; SDCC-LABEL: _get_bank:
; SDCC: ld a,#<(___bank_music)
define i8 @get_bank() {
  %addr = ptrtoint ptr @__bank_music to i16
  %bank = trunc i16 %addr to i8
  ret i8 %bank
}

; CHECK-LABEL: get_bank_hi:
; CHECK: ld a,z80_16hi(___bank_music)
; SDCC-LABEL: _get_bank_hi:
; SDCC: ld a,#>(___bank_music)
define i8 @get_bank_hi() {
  %addr = ptrtoint ptr @__bank_music to i16
  %hi16 = lshr i16 %addr, 8
  %hi = trunc i16 %hi16 to i8
  ret i8 %hi
}

; A ptrtoint straight to i8 legalizes through the same trunc.
; CHECK-LABEL: get_bank_direct:
; CHECK: ld a,z80_16lo(___bank_music)
define i8 @get_bank_direct() {
  %bank = ptrtoint ptr @__bank_music to i8
  ret i8 %bank
}

; The symbol also used as a real address: the full materialization must
; survive next to the folded byte load.
; CHECK-LABEL: bank_and_load:
; CHECK-DAG: z80_16lo(___bank_music)
; CHECK-DAG: ld {{hl|bc|de}},___bank_music
define i8 @bank_and_load() {
  %addr = ptrtoint ptr @__bank_music to i16
  %bank = trunc i16 %addr to i8
  %v = load i8, ptr @__bank_music
  %r = add i8 %bank, %v
  ret i8 %r
}

; llvm-readelf does not know the Z80 EM value, so match the raw relocation
; info: the low byte of the Info column is the type — 04 = R_Z80_ADDR16_LO,
; 05 = R_Z80_ADDR16_HI (llvm/include/llvm/BinaryFormat/ELFRelocs/Z80.def).
; RELOC-DAG: {{0[0-9a-f]}}04 {{.*}} ___bank_music
; RELOC-DAG: {{0[0-9a-f]}}05 {{.*}} ___bank_music
