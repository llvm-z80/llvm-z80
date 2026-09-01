// Bare-metal Z80/SM83 must never search the host's headers: glibc's
// stdint.h defines int32_t as int, which is 16 bits on these targets,
// silently truncating every int32_t (llvm-z80/llvm-z80#33).

// RUN: %clang --target=z80 -E -v %s 2>&1 | FileCheck %s
// RUN: %clang --target=sm83 -E -v %s 2>&1 | FileCheck %s
// RUN: %clang --target=z80-unknown-none-sdcc -E -v %s 2>&1 | FileCheck %s

// CHECK: #include <...> search starts here:
// CHECK-NOT: /usr/include
// CHECK-NOT: /usr/local/include
// CHECK: End of search list.
