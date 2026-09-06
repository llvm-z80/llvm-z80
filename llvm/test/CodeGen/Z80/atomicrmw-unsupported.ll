; RUN: not llc -mtriple=z80 -O1 < %s 2>&1 | FileCheck %s
; RUN: not llc -mtriple=sm83 -O1 < %s 2>&1 | FileCheck %s

; Nothing stops an interrupt between the load and the store, and the
; interrupt state cannot be reliably saved and restored to close that
; window, so atomic read-modify-write is refused instead of miscompiled.

@c = global i8 0

; CHECK: error:
; CHECK-SAME: atomic read-modify-write operations are not supported
define i8 @rmw() {
  %old = atomicrmw add ptr @c, i8 5 seq_cst
  ret i8 %old
}

; CHECK: error:
; CHECK-SAME: atomic read-modify-write operations are not supported
define i8 @cas() {
  %r = cmpxchg ptr @c, i8 0, i8 1 seq_cst seq_cst
  %old = extractvalue { i8, i1 } %r, 0
  ret i8 %old
}
