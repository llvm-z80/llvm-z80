// RUN: %clang_cc1 -triple z80 -internal-isystem %S/../../lib/Headers -emit-llvm -o - %s | FileCheck %s
//
// ravn/llvm-z80#42 — the compiler ships <intrinsic.h> for Z80, exposing the
// same intrinsic_* API as z88dk's <intrinsic.h>, so the SAME source compiles
// under both clang and SDCC with no per-source #ifdef.  Each wrapper lowers to
// the side-effecting llvm.z80.* intrinsic (no inline asm on the clang path).

#include <intrinsic.h>

// CHECK-LABEL: define {{.*}}void @crit()
// CHECK: call void @llvm.z80.di()
// CHECK: call void @llvm.z80.im2()
// CHECK: call void @llvm.z80.halt()
// CHECK: call void @llvm.z80.ei()
// CHECK: call void @llvm.z80.nop()
void crit(void) {
  intrinsic_di();
  intrinsic_im_2();
  intrinsic_halt();
  intrinsic_ei();
  intrinsic_nop();
}
