; RUN: not llc -mtriple=z80 -O2 %s -o /dev/null 2>&1 | FileCheck %s

; A runtime-selected (PHI'd / select'd) address_space(2) port is intentionally
; rejected. On Z80, IN r,(C) / OUT (C),r place register B on the upper address
; lines (A8-A15). On systems with 16-bit I/O decoding, emitting (C) without
; explicit control over B could cause unintended bus side-effects.
;
; The Legalizer still accepts the p2 PHI/select (P2 is legal for G_PHI and
; G_FREEZE) so it does not crash early; the diagnostic is a clean "cannot select"
; during instruction selection.

; CHECK: cannot select: {{.*}}G_STORE{{.*}}p2

@flag = external global i8

define void @wr5_phi(i8 %val) nounwind {
entry:
  %f = load i8, ptr @flag
  %cond = icmp ne i8 %f, 0
  %port = select i1 %cond, ptr addrspace(2) inttoptr (i16 11 to ptr addrspace(2)),
                          ptr addrspace(2) inttoptr (i16 10 to ptr addrspace(2))
  store volatile i8 %val, ptr addrspace(2) %port
  ret void
}
