; RUN: llc -mtriple=z80 -O1 < %s | FileCheck %s --check-prefixes=ON
; RUN: llc -mtriple=z80 -O1 -z80-static-frames=false < %s | FileCheck %s --check-prefixes=OFF
; RUN: llc -mtriple=z80 -O0 < %s | FileCheck %s --check-prefixes=OFF

; The static frame machinery must stay sound at its edges:
;
; * A function that codegen calls behind the IR's back (a block copy becomes
;   a memcpy call during selection) can be entered from every context, so a
;   module-local definition of it must keep its stack frame whenever a
;   second context exists.
; * A function the module analysis cannot reach (internal, never called,
;   address never taken) must keep its stack frame even when the input IR
;   already calls it norecurse, since the layout pass walks the same
;   reachable graph and would never resolve its placeholder operands.
; * A "nonreentrant" attribute arriving in the input IR while the feature
;   is disabled (flag or -O0) must lower as an ordinary stack frame instead
;   of leaving placeholder operands behind.

@src = global [32 x i8] zeroinitializer
@dst1 = global [32 x i8] zeroinitializer
@dst2 = global [32 x i8] zeroinitializer

; Both the interrupt context and the external one perform a block copy, so
; two activations of the module's own memcpy can be live at once.
; ON-NOT: memcpy.frame
define ptr @memcpy(ptr %d, ptr %s, i16 %n) norecurse {
entry:
  %saved = alloca ptr
  store volatile ptr %d, ptr %saved
  br label %loop
loop:
  %i = phi i16 [ 0, %entry ], [ %inc, %loop ]
  %sp = getelementptr i8, ptr %s, i16 %i
  %dp = getelementptr i8, ptr %d, i16 %i
  %b = load i8, ptr %sp
  store i8 %b, ptr %dp
  %inc = add i16 %i, 1
  %c = icmp ult i16 %inc, %n
  br i1 %c, label %loop, label %done
done:
  %r = load volatile ptr, ptr %saved
  ret ptr %r
}

define void @isr() "interrupt" {
  call void @llvm.memcpy.p0.p0.i16(ptr @dst1, ptr @src, i16 32, i1 false)
  ret void
}

define void @main() {
  call void @llvm.memcpy.p0.p0.i16(ptr @dst2, ptr @src, i16 32, i1 false)
  ret void
}

; Unreachable from anywhere the analysis can see, norecurse straight from
; the input. Must simply compile, on a stack frame.
; ON-NOT: dead.frame
define internal void @dead(i16 %x) norecurse {
  %s = alloca [8 x i16]
  %p = getelementptr [8 x i16], ptr %s, i16 0, i16 3
  store volatile i16 %x, ptr %p
  ret void
}

; The attribute is already present, but with the feature off nothing would
; resolve the placeholder operands it triggers. With the feature on the
; stale attribute is discarded and then legitimately re-derived, so the
; machinery demonstrably still fires for an ordinary function.
; ON: preattributed.frame
; OFF-NOT: .frame
; OFF-NOT: __static_frames
define void @preattributed(i16 %x) #0 {
  %s = alloca [8 x i16]
  %p = getelementptr [8 x i16], ptr %s, i16 0, i16 3
  store volatile i16 %x, ptr %p
  ret void
}

declare void @llvm.memcpy.p0.p0.i16(ptr, ptr, i16, i1)

attributes #0 = { norecurse "nonreentrant" }

; The frame aliases are defined after the last function, so check the
; excluded functions again past the positive match above.
; ON-NOT: memcpy.frame
; ON-NOT: dead.frame
