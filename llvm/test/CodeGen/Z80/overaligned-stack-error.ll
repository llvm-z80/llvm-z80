; RUN: not llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s 2>&1 | FileCheck %s
; RUN: not llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s 2>&1 | FileCheck %s
; RUN: not llc -verify-machineinstrs -mtriple=sm83 -O1 < %s 2>&1 | FileCheck %s

; SP sits at an arbitrary address on entry and SM83's SP-relative addressing
; rules out realigning it, so an object above the byte-aligned stack cannot
; be honored there. The backend refuses instead of silently handing out an
; unaligned address; a static object is the supported way to aligned data.

declare void @use(ptr)

; CHECK: error: {{.*}}in function oam_local{{.*}}: an over-aligned stack object (alignment 256, but the stack is byte-aligned)
define void @oam_local() {
  %buf = alloca [16 x i8], align 256
  call void @use(ptr %buf)
  ret void
}

; Even alignment 2 is above what the stack guarantees.
; CHECK: error: {{.*}}in function even_word{{.*}}: an over-aligned stack object (alignment 2, but the stack is byte-aligned)
define void @even_word() {
  %w = alloca i16, align 2
  call void @use(ptr %w)
  ret void
}
