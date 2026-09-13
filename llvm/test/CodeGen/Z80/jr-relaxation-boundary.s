# RUN: llvm-mc -triple=z80 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --triple=z80 --no-show-raw-insn %t.o | FileCheck %s
#
# A JR reaches 127 bytes forward and 128 back, counted from the end of the
# instruction. MC measures a fixup to the byte it patches, one short of that,
# so deciding on the unadjusted number relaxes a branch that still fits and
# lets one that no longer does through to the encoder as an error.

# CHECK-LABEL: <fwd_fits>:
# CHECK-NEXT: jr
fwd_fits:
  jr z,.Lf1
  .space 127
.Lf1:
  ret

# CHECK-LABEL: <fwd_relaxed>:
# CHECK-NEXT: jp
fwd_relaxed:
  jr z,.Lf2
  .space 128
.Lf2:
  ret

# CHECK-LABEL: <back_fits>:
# CHECK: jr
back_fits:
.Lb1:
  ret
  .space 125
  jr z,.Lb1

# CHECK-LABEL: <back_relaxed>:
# CHECK: jp
back_relaxed:
.Lb2:
  ret
  .space 126
  jr z,.Lb2
