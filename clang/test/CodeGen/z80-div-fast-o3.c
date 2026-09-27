// RUN: %clang_cc1 -triple z80 -O3 -S -mllvm -z80-asm-format=sdasz80 -o - %s | FileCheck %s --check-prefix=FAST
// RUN: %clang_cc1 -triple z80 -O2 -S -mllvm -z80-asm-format=sdasz80 -o - %s | FileCheck %s --check-prefix=SMALL
//
// At -O3, i16 div/mod libcalls are routed to the _fast variants which use a
// restoring-division loop optimised for throughput. At all other opt levels,
// the size-optimised routines are used unchanged. This test confirms the C ->
// asm pipeline selects the right variant based on opt level.
//
// NOTE: int / unsigned are i16 on this target.

int sdiv16(int a, int b) { return a / b; }
// FAST-LABEL:  _sdiv16:
// FAST:        call ___divhi3_fast
// SMALL-LABEL: _sdiv16:
// SMALL:       call ___divhi3
// SMALL-NOT:   ___divhi3_fast

unsigned udiv16(unsigned a, unsigned b) { return a / b; }
// FAST-LABEL:  _udiv16:
// FAST:        call ___udivhi3_fast
// SMALL-LABEL: _udiv16:
// SMALL:       call ___udivhi3
// SMALL-NOT:   ___udivhi3_fast
