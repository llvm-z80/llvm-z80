// The sdcc environment links SDCC's own library, found as sdcc finds it, and
// then <target>_sdcc_rt.lib.
// UNSUPPORTED: system-windows

// RUN: rm -rf %t && mkdir -p %t/bin %t/lib/z80 %t/lib/sm83 %t/share/sdcc/lib/z80 %t/share/sdcc/lib/sm83 %t/env/z80
// RUN: touch %t/bin/sdldz80 %t/bin/sdldgb && chmod +x %t/bin/sdldz80 %t/bin/sdldgb
// RUN: touch %t/share/sdcc/lib/z80/z80.lib %t/share/sdcc/lib/sm83/sm83.lib %t/env/z80/z80.lib
// RUN: touch %t/lib/z80/z80_sdcc_rt.lib %t/lib/sm83/sm83_sdcc_rt.lib

// Next to the installation the linker comes from.
// RUN: env -u SDCC_LIB -u SDCC_HOME PATH=%t/bin %clang --target=z80-unknown-none-sdcc -ccc-install-dir %t/bin -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=INSTALL-Z80
// RUN: env -u SDCC_LIB -u SDCC_HOME PATH=%t/bin %clang --target=sm83-nintendo-none-sdcc -ccc-install-dir %t/bin -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=INSTALL-SM83
// INSTALL-Z80: "-k" "{{.*}}/share/sdcc/lib/z80" "-l" "z80" "-k" "{{.*}}/lib/z80" "-l" "z80_sdcc_rt"
// INSTALL-SM83: "-k" "{{.*}}/share/sdcc/lib/sm83" "-l" "sm83" "-k" "{{.*}}/lib/sm83" "-l" "sm83_sdcc_rt"

// $SDCC_LIB comes first.
// RUN: env SDCC_LIB=%t/env PATH=%t/bin %clang --target=z80-unknown-none-sdcc -ccc-install-dir %t/bin -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=ENV
// ENV: "-k" "{{.*}}/env/z80" "-l" "z80" "-k" "{{.*}}/lib/z80" "-l" "z80_sdcc_rt"

// RUN: env SDCC_LIB=%t/env PATH=%t/bin %clang --target=z80-unknown-none-sdcc -ccc-install-dir %t/bin -nostdlib -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NOSTDLIB
// NOSTDLIB-NOT: "-l"

// Without SDCC's library, linking is an error.
// RUN: mkdir -p %t/none/bin && touch %t/none/bin/sdldz80 && chmod +x %t/none/bin/sdldz80
// RUN: not env -u SDCC_LIB -u SDCC_HOME PATH=%t/none/bin %clang --target=z80-unknown-none-sdcc --sysroot=%t/none -ccc-install-dir %t/none/bin -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=MISSING
// MISSING: error: cannot find SDCC's library 'z80.lib'; is SDCC installed?
