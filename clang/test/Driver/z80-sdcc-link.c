// The SDCC-toolchain link puts the runtime's crt0 before the inputs and hands
// sdldz80 the final .ihx path, since sdldz80 replaces whatever follows the
// last dot in the name it is given.

// RUN: %clang -### --target=z80-unknown-none-sdcc %s -o %t.dir/prog.c.tmp \
// RUN:   -ccc-install-dir %S/Inputs/z80_sdcc_tree/bin 2>&1 \
// RUN:   | FileCheck %s --check-prefix=Z80
// Z80: "{{[^"]*}}sdldz80" "-i" "{{[^"]*}}prog.c.tmp.ihx" "{{[^"]*}}z80_crt0.rel" "{{[^"]*}}.o" "-k" "{{[^"]*}}z80" "-l" "z80_rt"
// Z80-NEXT: "{{[^"]*}}mv" "{{[^"]*}}prog.c.tmp.ihx" "{{[^"]*}}prog.c.tmp"

// RUN: %clang -### --target=sm83-nintendo-none-sdcc %s -o %t.dir/prog \
// RUN:   -ccc-install-dir %S/Inputs/z80_sdcc_tree/bin 2>&1 \
// RUN:   | FileCheck %s --check-prefix=SM83
// SM83: "{{[^"]*}}sdldgb" "-i" "{{[^"]*}}prog.ihx" "{{[^"]*}}sm83_crt0.rel" "{{[^"]*}}.o" "-k" "{{[^"]*}}sm83" "-l" "sm83_rt"

// RUN: %clang -### --target=z80-unknown-none-sdcc %s -o %t.dir/prog -nostartfiles \
// RUN:   -ccc-install-dir %S/Inputs/z80_sdcc_tree/bin 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NOSTART
// NOSTART: "{{[^"]*}}sdldz80"
// NOSTART-NOT: crt0
// NOSTART-SAME: "-l" "z80_rt"

// Without -o the output keeps the name SDCC gives it.
// RUN: %clang -### --target=z80-unknown-none-sdcc %s \
// RUN:   -ccc-install-dir %S/Inputs/z80_sdcc_tree/bin 2>&1 \
// RUN:   | FileCheck %s --check-prefix=DEFAULT
// DEFAULT: "{{[^"]*}}sdldz80" "-i" "a.ihx"
// DEFAULT-NOT: "{{[^"]*}}mv"
