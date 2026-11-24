// RUN: %clang_cc1 -triple z80 -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,Z80
// RUN: %clang_cc1 -triple sm83 -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,SM83

// The attribute reaches the declaration that callers read, and the
// definition that saves the registers.

void ext(void) __attribute__((preserves_regs(b, c, iyl, iyh)));
void later(void);
void later(void) __attribute__((preserves_regs(h, l)));
void dropped(void) __attribute__((preserves_regs(b, bc)));

// CHECK: define{{.*}} void @body() [[BODY:#[0-9]+]]
void body(void) __attribute__((preserves_regs(d, e))) {
  ext();
  later();
  dropped();
}

// CHECK: declare{{.*}} void @ext() [[EXT:#[0-9]+]]
// CHECK: declare{{.*}} void @later() [[LATER:#[0-9]+]]
// CHECK: declare{{.*}} void @dropped() [[DROPPED:#[0-9]+]]

// CHECK-DAG: attributes [[BODY]] = {{.*}}"z80-preserves-regs"="d,e"
// Z80-DAG: attributes [[EXT]] = {{.*}}"z80-preserves-regs"="b,c,iyl,iyh"
// SM83-DAG: attributes [[EXT]] = {{.*}}"z80-preserves-regs"="b,c"{{[ }]}}
// CHECK-DAG: attributes [[LATER]] = {{.*}}"z80-preserves-regs"="h,l"
// CHECK-DAG: attributes [[DROPPED]] = {{.*}}"z80-preserves-regs"="b"
