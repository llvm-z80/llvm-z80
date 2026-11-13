; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O2 < %s | FileCheck %s

; address_space(2) represents Z80 port I/O. A compile-time-constant port selects
; the short constant instructions OUT (n),A / IN A,(n).
; Non-constant runtime ports fail instruction selection (see port-io-runtime-reject.ll).

define void @out_const(i8 %v) nounwind {
; CHECK-LABEL: _out_const:
; CHECK: out (5),a
  store volatile i8 %v, ptr addrspace(2) inttoptr (i16 5 to ptr addrspace(2))
  ret void
}

define i8 @in_const() nounwind {
; CHECK-LABEL: _in_const:
; CHECK: in a,(5)
  %v = load volatile i8, ptr addrspace(2) inttoptr (i16 5 to ptr addrspace(2))
  ret i8 %v
}

; Conditional store to two *constant* ports: the optimizer inlines the constant
; per branch, so each branch still uses OUT (n),A (no runtime port survives).
@flag = external global i8
define void @wr5(i8 %val) nounwind {
; CHECK-LABEL: _wr5:
; CHECK-DAG: out (10),a
; CHECK-DAG: out (11),a
entry:
  %f = load i8, ptr @flag
  %cond = icmp ne i8 %f, 0
  br i1 %cond, label %ifb, label %ifa

ifa:
  store volatile i8 %val, ptr addrspace(2) inttoptr (i16 10 to ptr addrspace(2))
  ret void

ifb:
  store volatile i8 %val, ptr addrspace(2) inttoptr (i16 11 to ptr addrspace(2))
  ret void
}
