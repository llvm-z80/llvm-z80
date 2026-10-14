; RUN: llc -mtriple=z80 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=sm83 -verify-machineinstrs < %s | FileCheck %s

; An inline asm memory operand takes its address in a register pair. Under
; pressure the allocator splits that address and asks which class the operand
; allows.

@v = global [8 x i16] zeroinitializer
@f = global i64 0

define i16 @t(i16 %a, i16 %b, i16 %c) {
; CHECK-LABEL: _t:
; CHECK:       ;APP
entry:
  %0 = load volatile i16, ptr @v
  %1 = load volatile i16, ptr getelementptr inbounds (i8, ptr @v, i16 2)
  %2 = load volatile i16, ptr getelementptr inbounds (i8, ptr @v, i16 4)
  %3 = load volatile i16, ptr getelementptr inbounds (i8, ptr @v, i16 6)
  %cmp18 = icmp sgt i16 %a, 0
  br i1 %cmp18, label %for.body, label %for.cond.cleanup

for.cond.cleanup:
  %x0.0.lcssa = phi i16 [ %0, %entry ], [ %add, %for.body ]
  %x1.0.lcssa = phi i16 [ %1, %entry ], [ %xor, %for.body ]
  %x2.0.lcssa = phi i16 [ %2, %entry ], [ %sub, %for.body ]
  %x3.0.lcssa = phi i16 [ %3, %entry ], [ %add2, %for.body ]
  %add3 = add i16 %c, %b
  %add4 = add i16 %add3, %x0.0.lcssa
  %add5 = add i16 %add4, %x1.0.lcssa
  %add6 = add i16 %add5, %x2.0.lcssa
  %add7 = add i16 %add6, %x3.0.lcssa
  ret i16 %add7

for.body:
  %i.023 = phi i16 [ %inc, %for.body ], [ 0, %entry ]
  %x3.022 = phi i16 [ %add2, %for.body ], [ %3, %entry ]
  %x2.021 = phi i16 [ %sub, %for.body ], [ %2, %entry ]
  %x1.020 = phi i16 [ %xor, %for.body ], [ %1, %entry ]
  %x0.019 = phi i16 [ %add, %for.body ], [ %0, %entry ]
  call void asm sideeffect "", "=*m,*m"(ptr elementtype(i64) @f, ptr elementtype(i64) @f)
  %call = call i16 @g(i16 %x1.020)
  %add = add i16 %call, %x0.019
  %add1 = add i16 %i.023, %x2.021
  %xor = xor i16 %add1, %x1.020
  %sub = sub i16 %x2.021, %x3.022
  %add2 = add i16 %add, %x3.022
  call void asm sideeffect "", "=*m,*m"(ptr elementtype(i64) @f, ptr elementtype(i64) @f)
  %inc = add i16 %i.023, 1
  %exitcond.not = icmp eq i16 %inc, %a
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body
}

declare i16 @g(i16)
