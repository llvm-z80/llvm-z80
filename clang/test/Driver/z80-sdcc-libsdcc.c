// The sdcc environment links SDCC's own library, found as sdcc finds it.
// UNSUPPORTED: system-windows

// RUN: rm -rf %t && mkdir -p %t/bin %t/share/sdcc/lib/z80 %t/share/sdcc/lib/sm83 %t/env/z80
// RUN: touch %t/bin/sdldz80 %t/bin/sdldgb && chmod +x %t/bin/sdldz80 %t/bin/sdldgb
// RUN: touch %t/share/sdcc/lib/z80/z80.lib %t/share/sdcc/lib/sm83/sm83.lib %t/env/z80/z80.lib

// Next to the installation the linker comes from.
// RUN: env -u SDCC_LIB -u SDCC_HOME PATH=%t/bin %clang --target=z80-unknown-none-sdcc -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=INSTALL-Z80
// RUN: env -u SDCC_LIB -u SDCC_HOME PATH=%t/bin %clang --target=sm83-nintendo-none-sdcc -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=INSTALL-SM83
// INSTALL-Z80: "-k" "{{.*}}/share/sdcc/lib/z80" "-l" "z80"
// INSTALL-SM83: "-k" "{{.*}}/share/sdcc/lib/sm83" "-l" "sm83"

// $SDCC_LIB comes first.
// RUN: env SDCC_LIB=%t/env PATH=%t/bin %clang --target=z80-unknown-none-sdcc -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=ENV
// ENV: "-k" "{{.*}}/env/z80" "-l" "z80"

// RUN: env SDCC_LIB=%t/env PATH=%t/bin %clang --target=z80-unknown-none-sdcc -nostdlib -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NOSTDLIB
// NOSTDLIB-NOT: "-l" "z80"
