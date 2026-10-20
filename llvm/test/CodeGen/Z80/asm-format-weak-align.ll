; RUN: llc -mtriple=z80-unknown-none-sdcc < %s 2>/dev/null \
; RUN:   | FileCheck %s --check-prefix=SDCC --implicit-check-not=.weak \
; RUN:     --implicit-check-not=p2align
; RUN: llc -mtriple=sm83-unknown-none-sdcc < %s 2>/dev/null \
; RUN:   | FileCheck %s --check-prefix=SDCC --implicit-check-not=.weak \
; RUN:     --implicit-check-not=p2align
; RUN: llc -mtriple=z80 -z80-asm-format=z88dk < %s 2>/dev/null \
; RUN:   | FileCheck %s --check-prefix=Z88DK --implicit-check-not=.weak \
; RUN:     --implicit-check-not=ALIGN
; RUN: llc -mtriple=z80-unknown-none-sdcc < %s -o /dev/null 2>&1 \
; RUN:   | FileCheck %s --check-prefix=WARN -DFORMAT=sdasz80
; RUN: llc -mtriple=z80 -z80-asm-format=z88dk < %s -o /dev/null 2>&1 \
; RUN:   | FileCheck %s --check-prefix=WARN -DFORMAT=z88dk

; Weak symbols are emitted as global and alignment is dropped, each with a
; warning. Weak data with an initial value goes through the SDCC initializer
; areas.

; WARN-DAG: warning: weak linkage of 'weak_data' is ignored in the [[FORMAT]] format
; WARN-DAG: warning: weak linkage of 'weak_ref' is ignored in the [[FORMAT]] format
; WARN-DAG: warning: weak linkage of 'weak_func' is ignored in the [[FORMAT]] format
; WARN-DAG: warning: alignment of 'aligned_func' is ignored in the [[FORMAT]] format
; WARN-DAG: warning: alignment of 'aligned_data' is ignored in the [[FORMAT]] format

@weak_data = weak global i16 4660
@aligned_data = constant [2 x i16] [i16 1, i16 2], align 4
@weak_ref = extern_weak global i16

; SDCC:      .globl _weak_func
; SDCC-NEXT: _weak_func:
; Z88DK:     GLOBAL _weak_func
; Z88DK-NEXT: _weak_func:
define weak i16 @weak_func() {
  ret i16 7
}

; SDCC:      .globl _aligned_func
; SDCC-NEXT: _aligned_func:
; Z88DK:     GLOBAL _aligned_func
; Z88DK-NEXT: _aligned_func:
define void @aligned_func() align 256 {
  ret void
}

define ptr @use_weak_ref() {
  ret ptr @weak_ref
}

; SDCC:      .area _INITIALIZED
; SDCC-NEXT: .globl _weak_data
; SDCC-NEXT: _weak_data:
; SDCC:      .area _INITIALIZER
; SDCC-NEXT: .dw 4660
; Z88DK:     GLOBAL _weak_data
; Z88DK-NEXT: _weak_data:

; SDCC:      .globl _aligned_data
; SDCC-NEXT: _aligned_data:
; Z88DK:     GLOBAL _aligned_data
; Z88DK-NEXT: _aligned_data:

; SDCC:      .globl _weak_ref
; Z88DK:     GLOBAL _weak_ref
