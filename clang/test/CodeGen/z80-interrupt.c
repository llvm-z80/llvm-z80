// RUN: %clang_cc1 -triple z80 -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,Z80
// RUN: %clang_cc1 -triple sm83 -emit-llvm %s -o - | FileCheck %s

// CHECK: define{{.*}} void @isr() [[ISR:#[0-9]+]]
__attribute__((interrupt)) void isr(void) {}

// CHECK: define{{.*}} void @isr_nested() [[NESTED:#[0-9]+]]
__attribute__((interrupt("nested"))) void isr_nested(void) {}

#ifdef __Z80__
// Z80: define{{.*}} void @isr_nmi() [[NMI:#[0-9]+]]
__attribute__((interrupt("nmi"))) void isr_nmi(void) {}
#endif

// CHECK: attributes [[ISR]] = {{.*}}"interrupt" {{.*}}
// CHECK: attributes [[NESTED]] = {{.*}}"interrupt"="nested"
// Z80: attributes [[NMI]] = {{.*}}"interrupt"="nmi"
