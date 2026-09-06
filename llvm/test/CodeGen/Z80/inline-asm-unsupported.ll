; RUN: not llc -mtriple=z80 -O1 < %s 2>&1 | FileCheck %s

; A value wider than a 16-bit register pair has no register placement, and
; the lowering cannot split wide direct asm operands, so these must be
; rejected with a diagnostic instead of crashing.

; CHECK: error:
; CHECK-SAME: unsupported inline asm operand: value wider than 16 bits
define void @wide_input(i32 %v) {
  call void asm sideeffect "", "r"(i32 %v)
  ret void
}

; CHECK: error:
; CHECK-SAME: unsupported inline asm operand: value wider than 16 bits
define i32 @wide_output() {
  %r = call i32 asm "", "=r"()
  ret i32 %r
}

; CHECK: error:
; CHECK-SAME: unsupported inline asm operand: value wider than 16 bits
define void @aggregate_indirect(ptr %p) {
  call void asm "", "=*r"(ptr elementtype({ float, float }) %p)
  ret void
}
