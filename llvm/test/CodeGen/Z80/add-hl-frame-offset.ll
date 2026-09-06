; RUN: llc -verify-machineinstrs -mtriple=z80 -O0 -stop-after=finalize-isel < %s \
; RUN:   -o - | FileCheck %s

; Folding a frame slot load into the 16-bit add gives the pseudo the slot and
; the displacement within it, so both belong in its operand list.

; CHECK: ADD_HL_FI %stack.{{[0-9]+}}{{[^,]*}}, {{[1-9][0-9]*}}

define i16 @add_from_slot(i16 %a, i16 %b, i16 %c) "frame-pointer"="all" {
entry:
  %buf = alloca [8 x i16], align 1
  %p0 = getelementptr inbounds [8 x i16], ptr %buf, i16 0, i16 0
  %p1 = getelementptr inbounds [8 x i16], ptr %buf, i16 0, i16 1
  %p2 = getelementptr inbounds [8 x i16], ptr %buf, i16 0, i16 2
  %p3 = getelementptr inbounds [8 x i16], ptr %buf, i16 0, i16 3
  store i16 %a, ptr %p0, align 1
  store i16 %b, ptr %p1, align 1
  store i16 %c, ptr %p2, align 1
  store i16 %a, ptr %p3, align 1
  %l0 = load i16, ptr %p0, align 1
  %l1 = load i16, ptr %p1, align 1
  %l2 = load i16, ptr %p2, align 1
  %l3 = load i16, ptr %p3, align 1
  %s0 = add i16 %l0, %l1
  %s1 = add i16 %s0, %l2
  %s2 = add i16 %s1, %l3
  ret i16 %s2
}
