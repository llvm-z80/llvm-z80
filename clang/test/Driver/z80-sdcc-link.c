// The SDCC-toolchain link places code at 0x0200 and data at 0x8000 or 0xc000,
// and puts SDCC's crt0 before the inputs. sdldz80 replaces whatever follows the
// last dot in the output name. The driver therefore passes the final .ihx path.

// RUN: env -u SDCC_LIB SDCC_HOME=%S/Inputs/z80_sdcc_tree \
// RUN:   %clang -### --target=z80-unknown-none-sdcc %s -o %t.dir/prog.c.tmp \
// RUN:   -ccc-install-dir %S/Inputs/z80_sdcc_tree/bin 2>&1 \
// RUN:   | FileCheck %s --check-prefix=Z80
// Z80: "{{[^"]*}}sdldz80" "-i" "{{[^"]*}}prog.c.tmp.ihx" "-b" "_CODE=0x0200" "-b" "_DATA=0x8000" "{{[^"]*}}z80{{/|\\\\}}crt0.rel" "{{[^"]*}}.o" "-k" "{{[^"]*}}z80" "-l" "z80" "-k" "{{[^"]*}}z80" "-l" "z80_sdcc_rt"
// Z80-NEXT: "{{[^"]*}}mv" "{{[^"]*}}prog.c.tmp.ihx" "{{[^"]*}}prog.c.tmp"

// RUN: env -u SDCC_LIB SDCC_HOME=%S/Inputs/z80_sdcc_tree \
// RUN:   %clang -### --target=sm83-nintendo-none-sdcc %s -o %t.dir/prog \
// RUN:   -ccc-install-dir %S/Inputs/z80_sdcc_tree/bin 2>&1 \
// RUN:   | FileCheck %s --check-prefix=SM83
// SM83: "{{[^"]*}}sdldgb" "-i" "{{[^"]*}}prog.ihx" "-b" "_CODE=0x0200" "-b" "_DATA=0xc000" "{{[^"]*}}sm83{{/|\\\\}}crt0.rel" "{{[^"]*}}.o" "-k" "{{[^"]*}}sm83" "-l" "sm83" "-k" "{{[^"]*}}sm83" "-l" "sm83_sdcc_rt"

// Without crt0 the areas keep the same places. A later -b moves the areas.
// RUN: env -u SDCC_LIB SDCC_HOME=%S/Inputs/z80_sdcc_tree \
// RUN:   %clang -### --target=z80-unknown-none-sdcc %s -o %t.dir/prog -nostartfiles \
// RUN:   -Wl,-b,_CODE=0x8000 -ccc-install-dir %S/Inputs/z80_sdcc_tree/bin 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NOSTART
// NOSTART: "{{[^"]*}}sdldz80" {{.*}} "-b" "_CODE=0x0200" "-b" "_DATA=0x8000"
// NOSTART-NOT: crt0
// NOSTART-SAME: "-l" "z80_sdcc_rt" "-b" "_CODE=0x8000"

// Without -o the output keeps the name SDCC gives it.
// RUN: env -u SDCC_LIB SDCC_HOME=%S/Inputs/z80_sdcc_tree \
// RUN:   %clang -### --target=z80-unknown-none-sdcc %s \
// RUN:   -ccc-install-dir %S/Inputs/z80_sdcc_tree/bin 2>&1 \
// RUN:   | FileCheck %s --check-prefix=DEFAULT
// DEFAULT: "{{[^"]*}}sdldz80" "-i" "a.ihx"
// DEFAULT-NOT: "{{[^"]*}}mv"
