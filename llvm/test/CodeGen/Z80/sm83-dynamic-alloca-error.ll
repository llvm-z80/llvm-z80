; RUN: not llc -mtriple=sm83 -O1 < %s 2>&1 | FileCheck %s
; RUN: not llc -mtriple=sm83 -O0 < %s 2>&1 | FileCheck %s

; SM83 has no frame pointer register, so a stack allocation whose size is
; known only at run time is refused with a diagnostic, not a crash.

declare void @use(ptr)

; CHECK: error:
; CHECK-SAME: dynamic stack allocation (alloca/VLA) is not supported on SM83
; CHECK-NOT: Stack dump
define void @vla(i16 %n) {
  %buf = alloca i8, i16 %n
  call void @use(ptr %buf)
  ret void
}
