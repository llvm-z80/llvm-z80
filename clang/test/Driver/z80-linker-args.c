// The linker gets the values of -Wl and -Xlinker in order, without the option
// names.

// RUN: %clang -### --target=z80 %s -o %t -Wl,--gc-sections,-Map=%t.map \
// RUN:   -Xlinker --print-gc-sections 2>&1 | FileCheck %s --check-prefix=ELF
// ELF: "{{[^"]*}}ld.lld"
// ELF-SAME: "--gc-sections" "-Map={{[^"]*}}.map" "--print-gc-sections"

// RUN: %clang -### --target=z80-unknown-none-sdcc %s -o %t -Wl,-b,_CODE=0x100 \
// RUN:   -Xlinker -m 2>&1 | FileCheck %s --check-prefix=SDCC
// SDCC: "{{[^"]*}}sdldz80"
// SDCC-SAME: "-b" "_CODE=0x100" "-m"
