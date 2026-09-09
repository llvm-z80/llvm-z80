; RUN: not llc -mtriple=z80 -O2 %s -o /dev/null 2>&1 | FileCheck %s

; A runtime-selected (PHI'd / select'd) address_space(2) port is intentionally
; NOT supported.  It must FAIL TO SELECT rather than silently emit OUT (C),A,
; whose "B on the high address bits" behaviour is only coincidentally harmless
; on RC700 (which decodes just the low 8 port bits).  Baking that hardware
; assumption into codegen would be a silent-miscompile trap, so we reject it.
;
; The Legalizer must still ACCEPT the p2 PHI/select (P2 is legal for G_PHI /
; G_FREEZE) -- i.e. it must not crash; the diagnostic is a clean "cannot select"
; at instruction selection.  (ravn/llvm-z80 #44.)

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
