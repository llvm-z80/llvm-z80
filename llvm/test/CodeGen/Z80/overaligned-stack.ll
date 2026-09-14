; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 < %s 2>&1 | FileCheck %s
; RUN: llc -verify-machineinstrs -mtriple=sm83 -O1 < %s 2>&1 | FileCheck %s
; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 -z80-warn-stack-align-padding=0 \
; RUN:   < %s 2>&1 | FileCheck %s --check-prefix=QUIET

; The stack is byte-aligned, so an object wanting more is over-allocated by
; its alignment minus one and its address rounded up inside that room.

declare void @use(ptr)

; CHECK: warning: {{.*}}in function even_word{{.*}}: aligning a stack object to 2 costs 1 byte of padding; use a static object for aligned data
; QUIET-NOT: warning:
define void @even_word() {
  %w = alloca i16, align 2
  call void @use(ptr %w)
  ret void
}

; CHECK: warning: {{.*}}in function widest{{.*}}: aligning a stack object to 128 costs 127 bytes of padding; use a static object for aligned data
define void @widest() {
  %b = alloca [16 x i8], align 128
  call void @use(ptr %b)
  ret void
}

; A byte-aligned object is left alone.
; CHECK-NOT: aligning a stack object to 1
define void @plain() {
  %b = alloca [16 x i8], align 1
  call void @use(ptr %b)
  ret void
}

; llvm.lifetime.* only takes an alloca, so those uses have to stay on the
; object rather than follow the rounded address into it.
; CHECK: warning: {{.*}}in function lifetime{{.*}}: aligning a stack object to 2
define void @lifetime() {
  %w = alloca i16, align 2
  call void @llvm.lifetime.start.p0(ptr nonnull %w)
  call void @use(ptr %w)
  call void @llvm.lifetime.end.p0(ptr nonnull %w)
  ret void
}
