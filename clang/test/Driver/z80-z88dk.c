// The z88dk environment emits z80asm syntax and assembles with z88dk-z80asm.
// RUN: %clang --target=z80-unknown-none-z88dk -c -### %s 2>&1 | FileCheck %s
// RUN: not %clang --target=sm83-nintendo-none-z88dk -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=SM83

// CHECK: "-cc1"{{.*}} "-mllvm" "-z80-asm-format=z88dk"
// CHECK: "{{[^"]*}}z88dk-z80asm" "-o={{[^"]*}}.o" "{{[^"]*}}.s"

// SM83: error: unknown target triple 'sm83-nintendo-none-z88dk'
