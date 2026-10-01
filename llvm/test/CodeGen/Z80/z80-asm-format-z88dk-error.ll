; RUN: not llc -mtriple=z80 -z80-asm-format=z88dk < %s -o /dev/null 2>&1 \
; RUN:   | FileCheck %s

; A section with no z88dk name is an error.

; CHECK: error: section '.init_array' cannot be emitted in the z88dk format
; CHECK: error: section '.mysec' cannot be emitted in the z88dk format

@llvm.global_ctors = appending global [1 x { i32, ptr, ptr }] [{ i32, ptr, ptr } { i32 65535, ptr @init, ptr null }]
@in_mysec = global i8 1, section ".mysec"

define void @init() {
  ret void
}
