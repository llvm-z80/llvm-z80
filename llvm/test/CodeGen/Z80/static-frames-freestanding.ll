; RUN: llc -mtriple=z80 -O1 -z80-static-frames < %s | FileCheck %s --check-prefix=CLOSED
; RUN: sed 's|"Freestanding", i32 1|"Freestanding", i32 0|' %s | llc -mtriple=z80 -O1 -z80-static-frames | FileCheck %s --check-prefix=OPEN

; In an open world, an external call from a function with external linkage creates
; a synthetic cycle through CallsExternalNode -> ExternalCallingNode, preventing
; the recursion analysis from proving non-reentrancy.
; In a closed-world / freestanding environment, foreign code does not re-enter
; module symbols, so single-node SCCs are proven non-recursive and receive static frames.
; The freestanding assumption comes from the "Freestanding" module flag that
; clang emits under -ffreestanding (CodeGenModule.cpp: LangOpts.Freestanding).

declare void @ext_helper()

; OPEN-LABEL: f:
; OPEN-NOT: L_f.frame
; CLOSED-LABEL: f:
; CLOSED: L_f.frame
define void @f(i16 %x) {
  %buf = alloca [4 x i16], align 1
  store volatile i16 %x, ptr %buf, align 1
  call void @ext_helper()
  ret void
}

!llvm.module.flags = !{!0}
!0 = !{i32 2, !"Freestanding", i32 1}
