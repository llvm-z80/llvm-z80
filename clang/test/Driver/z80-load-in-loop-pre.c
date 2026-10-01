// Load PRE in loops is off for Z80/SM83 and a user's -mllvm can override it.

// RUN: %clang -### --target=z80 -c %s 2>&1 | FileCheck %s
// RUN: %clang -### --target=sm83 -c %s 2>&1 | FileCheck %s
// RUN: %clang -### --target=z80-unknown-none-sdcc -c %s 2>&1 | FileCheck %s
// CHECK: "-mllvm" "-enable-load-in-loop-pre=false"

// RUN: %clang -### --target=z80 -mllvm -enable-load-in-loop-pre=true -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=USER
// USER: "-enable-load-in-loop-pre=false"
// USER-SAME: "-mllvm" "-enable-load-in-loop-pre=true"
