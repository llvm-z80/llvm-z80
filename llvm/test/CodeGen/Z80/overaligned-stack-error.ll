; RUN: not llc -verify-machineinstrs -mtriple=z80 -O1 < %s -o /dev/null 2>&1 | FileCheck %s
; RUN: not llc -verify-machineinstrs -mtriple=z80 -O0 < %s -o /dev/null 2>&1 | FileCheck %s
; RUN: not llc -verify-machineinstrs -mtriple=sm83 -O1 < %s -o /dev/null 2>&1 | FileCheck %s

; The rounding masks the low byte of the address, so that is as far as an
; alignment can reach.

declare void @use(ptr)

; CHECK: error: {{.*}}in function oam_local{{.*}}: cannot align a stack object to 256, the maximum is 128; use a static object for aligned data
define void @oam_local() {
  %buf = alloca [16 x i8], align 256
  call void @use(ptr %buf)
  ret void
}
