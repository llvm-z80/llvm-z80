; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=z80 -O0 -verify-machineinstrs < %s -o /dev/null

; A store whose value is wider than its memory type legalizes to a
; truncate plus a store at memory width. Wide bitfields produce these:
; the tail byte of a 40-bit field store arrives as an s16 value with
; 8-bit memory.

@x = global [6 x i8] zeroinitializer, align 1

; CHECK-LABEL: store40:
; CHECK: ret
define void @store40(i64 %v) {
  %t = trunc i64 %v to i40
  store i40 %t, ptr @x, align 1
  ret void
}

; CHECK-LABEL: store24:
; CHECK: ret
define void @store24(i32 %v) {
  %t = trunc i32 %v to i24
  store i24 %t, ptr @x, align 1
  ret void
}

; CHECK-LABEL: rmw24:
; CHECK: ret
define void @rmw24() {
  %old = load i24, ptr @x, align 1
  %inc = add i24 %old, 1
  store i24 %inc, ptr @x, align 1
  ret void
}
