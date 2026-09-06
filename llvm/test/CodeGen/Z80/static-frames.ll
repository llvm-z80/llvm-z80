; RUN: llc -mtriple=z80 -O1 -z80-static-frames -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -O1 -z80-static-frames -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=z80 -O1 -verify-machineinstrs < %s -o /dev/null

; The frames of provably non-reentrant functions move to static memory:
; no prologue, every slot access an absolute address. Recursive functions
; and functions shared between execution contexts keep their stack frames.

@sink = global ptr null

; A leaf whose local is only kept alive by volatile accesses still gets its
; slots out of the stack.
; CHECK-LABEL: leaf:
; CHECK-NOT: push ix
; CHECK: L_leaf.frame
; CHECK: ret
define internal i16 @leaf(i16 %x) {
  %buf = alloca [4 x i16], align 1
  store volatile i16 %x, ptr %buf, align 1
  %v = load volatile i16, ptr %buf, align 1
  ret i16 %v
}

; Recursion keeps the stack frame: no frame symbol appears.
; CHECK-LABEL: recurse:
; CHECK-NOT: L_recurse.frame
define internal i16 @recurse(i16 %n) {
entry:
  %buf = alloca i16, align 1
  store volatile i16 %n, ptr %buf, align 1
  %c = icmp eq i16 %n, 0
  br i1 %c, label %done, label %again
again:
  %m = sub i16 %n, 1
  %r = call i16 @recurse(i16 %m)
  br label %done
done:
  %v = load volatile i16, ptr %buf, align 1
  ret i16 %v
}

; Reachable from both main and the interrupt handler: stack frame.
; CHECK-LABEL: shared:
; CHECK-NOT: L_shared.frame
define internal i16 @shared(i16 %x) {
  %buf = alloca i16, align 1
  store volatile i16 %x, ptr %buf, align 1
  %v = load volatile i16, ptr %buf, align 1
  ret i16 %v
}

; Address-taken: under the closed-world premise only main-context code can
; call through the pointer, so a single activation is still guaranteed.
; CHECK-LABEL: pointed:
; CHECK: L_pointed.frame
define internal i16 @pointed(i16 %x) {
  %buf = alloca i16, align 1
  store volatile i16 %x, ptr %buf, align 1
  %v = load volatile i16, ptr %buf, align 1
  ret i16 %v
}

; The handler itself is a context root with a frame of its own.
; CHECK-LABEL: isr:
; CHECK: L_isr.frame
define void @isr() #0 {
  %buf = alloca i16, align 1
  store volatile i16 7, ptr %buf, align 1
  %r = call i16 @shared(i16 3)
  ret void
}

; CHECK-LABEL: main:
; CHECK: L_main.frame
define i16 @main() {
  %buf = alloca [8 x i16], align 1
  store volatile i16 1, ptr %buf, align 1
  store ptr @pointed, ptr @sink, align 1
  %a = call i16 @leaf(i16 4)
  %b = call i16 @recurse(i16 3)
  %c = call i16 @shared(i16 5)
  %s1 = add i16 %a, %b
  %s2 = add i16 %s1, %c
  ret i16 %s2
}

; Every frame is carved out of one block.
; CHECK: L___static_frames
; CHECK-DAG: L_main.frame = L___static_frames
; CHECK-DAG: L_isr.frame = L___static_frames

attributes #0 = { "interrupt" }
