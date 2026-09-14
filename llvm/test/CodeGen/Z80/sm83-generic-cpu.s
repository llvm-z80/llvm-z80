# RUN: llvm-mc -triple=sm83 -mcpu=generic -filetype=obj %s -o /dev/null
# RUN: llvm-mc -triple=sm83 -filetype=obj %s -o /dev/null
# RUN: not llvm-mc -triple=z80 -mcpu=generic -filetype=obj %s -o /dev/null 2>&1 \
# RUN:   | FileCheck %s
#
# "generic" is the triple's default CPU, which is neither a CPU short of the
# triple's own instructions nor SM83's handed to a plain Z80.

  ldh (0x44),a
  ld (hl+),a
  ldhl sp,4

# CHECK: instruction requires a CPU feature not currently enabled
