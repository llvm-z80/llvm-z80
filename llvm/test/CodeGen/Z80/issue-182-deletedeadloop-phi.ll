; RUN: llc -mtriple=z80 -O1 < %s 2>&1 | FileCheck %s
; XFAIL: *
;
; DEMONSTRATION TEST (ravn/llvm-z80#182) — expected to FAIL (crash) on current
; upstream; the CHECK asserts the POST-FIX behavior.  Remove the `XFAIL` line
; once the deleteDeadLoop fix lands.
;
; `deleteDeadLoop` (llvm/lib/Transforms/Utils/LoopUtils.cpp) malforms SSA on the
; exit block when the exit block has phi entries from OUTSIDE the deleted loop
; -- specifically when the deleted loop's exit is the header of ANOTHER loop
; with its own backedge phi entries.  The original code keeps phi entry 0 and
; removes the rest, but entry 0 may be the other loop's backedge entry, not the
; exiting-block entry.  Result: self-referencing instructions (`%v = add %v, 1`
; outside any phi), which SCEV's createSCEVIter then walks into unbounded
; worklist growth ("SmallVector unable to grow") at LoopDeletionPass.
;
; Reachable from clean source: two sequential loops over the same array, where
; the first loop is rewritten to a fill by Z80LoopIdiomFill and then deleted.

@a = dso_local global [100 x i8] zeroinitializer, align 1

; CHECK-LABEL: g:
; CHECK-NOT: SmallVector unable to grow

define dso_local void @g() {
entry:
  br label %loop1

loop1:                                          ; preds = %entry, %loop1
  %i1 = phi i16 [ 0, %entry ], [ %i1.next, %loop1 ]
  %gep1 = getelementptr i8, ptr @a, i16 %i1
  store i8 0, ptr %gep1, align 1
  %i1.next = add nuw nsw i16 %i1, 1
  %done1 = icmp eq i16 %i1.next, 100
  br i1 %done1, label %loop2, label %loop1

loop2:                                          ; preds = %loop1, %loop2
  %i2 = phi i16 [ 0, %loop1 ], [ %i2.next, %loop2 ]
  %gep2 = getelementptr i8, ptr @a, i16 %i2
  %v = load i8, ptr %gep2, align 1
  %v.inc = add i8 %v, 1
  store i8 %v.inc, ptr %gep2, align 1
  %i2.next = add nuw nsw i16 %i2, 1
  %done2 = icmp eq i16 %i2.next, 100
  br i1 %done2, label %exit, label %loop2

exit:                                           ; preds = %loop2
  ret void
}
