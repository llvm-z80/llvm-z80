// RUN: %clang --target=avr -c -### %s -mdouble=64 2>&1 | FileCheck %s
// RUN: %clang --target=z80 -c -### %s -mdouble=64 2>&1 | FileCheck %s
// RUN: %clang --target=z80 -c -### %s -mdouble=32 2>&1 | FileCheck --check-prefix=CHECK-32 %s
// RUN: %clang --target=sm83 -c -### %s -mdouble=64 2>&1 | FileCheck %s
// RUN: %clang --target=sm83 -c -### %s -mdouble=32 2>&1 | FileCheck --check-prefix=CHECK-32 %s

// CHECK: "-mdouble=64"
// CHECK-32: "-mdouble=32"

// RUN: not %clang --target=aarch64 -c -### %s -mdouble=64 2>&1 | FileCheck --check-prefix=ERR %s

// ERR: error: unsupported option '-mdouble=64' for target 'aarch64'
