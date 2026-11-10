// The sdcc environment links SDCC's crt0 and library, and then
// <target>_sdcc_rt.lib.
// UNSUPPORTED: system-windows

// A Z80 library from SDCC 4.2 or later defines __mulsint2slong.
// RUN: rm -rf %t && mkdir -p %t/bin %t/lib/z80 %t/lib/sm83 %t/share/sdcc/lib/z80 %t/share/sdcc/lib/sm83 %t/env/z80
// RUN: touch %t/bin/sdldz80 %t/bin/sdldgb && chmod +x %t/bin/sdldz80 %t/bin/sdldgb
// RUN: echo 'S ___mulsint2slong Def0000' > %t/share/sdcc/lib/z80/z80.lib
// RUN: echo 'S ___mulsint2slong Def0000' > %t/env/z80/z80.lib
// RUN: touch %t/share/sdcc/lib/sm83/sm83.lib
// RUN: touch %t/lib/z80/z80_sdcc_rt.lib %t/lib/sm83/sm83_sdcc_rt.lib

// Next to the installation the linker comes from.
// RUN: env -u SDCC_LIB -u SDCC_HOME PATH=%t/bin %clang --target=z80-unknown-none-sdcc -ccc-install-dir %t/bin -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=INSTALL-Z80
// RUN: env -u SDCC_LIB -u SDCC_HOME PATH=%t/bin %clang --target=sm83-nintendo-none-sdcc -ccc-install-dir %t/bin -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=INSTALL-SM83
// INSTALL-Z80: "-k" "{{.*}}/share/sdcc/lib/z80" "-l" "z80" "-k" "{{.*}}/lib/z80" "-l" "z80_sdcc_rt"
// INSTALL-SM83: "-k" "{{.*}}/share/sdcc/lib/sm83" "-l" "sm83" "-k" "{{.*}}/lib/sm83" "-l" "sm83_sdcc_rt"

// SDCC built for Windows keeps the library in lib.
// RUN: mkdir -p %t/clang/bin %t/clang/lib/z80 %t/win/bin %t/win/lib/z80
// RUN: touch %t/clang/lib/z80/z80_sdcc_rt.lib %t/win/bin/sdldz80 && chmod +x %t/win/bin/sdldz80
// RUN: echo 'S ___mulsint2slong Def0000' > %t/win/lib/z80/z80.lib
// RUN: env -u SDCC_LIB -u SDCC_HOME PATH=%t/win/bin %clang --target=z80-unknown-none-sdcc -ccc-install-dir %t/clang/bin -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=WIN
// RUN: env -u SDCC_LIB SDCC_HOME=%t/win PATH=%t/bin %clang --target=z80-unknown-none-sdcc -ccc-install-dir %t/clang/bin -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=WIN
// WIN: "{{.*}}/win/lib/z80/crt0.rel" {{.*}} "-k" "{{.*}}/win/lib/z80" "-l" "z80"

// $SDCC_LIB comes first.
// RUN: env SDCC_LIB=%t/env PATH=%t/bin %clang --target=z80-unknown-none-sdcc -ccc-install-dir %t/bin -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=ENV
// ENV: "{{.*}}/env/z80/crt0.rel" {{.*}} "-k" "{{.*}}/env/z80" "-l" "z80" "-k" "{{.*}}/lib/z80" "-l" "z80_sdcc_rt"

// -nodefaultlibs keeps SDCC's crt0 and the directory of SDCC's library.
// RUN: env SDCC_LIB=%t/env PATH=%t/bin %clang --target=z80-unknown-none-sdcc -ccc-install-dir %t/bin -nodefaultlibs -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NODEFAULTLIBS
// NODEFAULTLIBS: "{{.*}}/env/z80/crt0.rel" {{.*}} "-k" "{{.*}}/env/z80"
// NODEFAULTLIBS-NOT: "-l"

// RUN: env SDCC_LIB=%t/env PATH=%t/bin %clang --target=z80-unknown-none-sdcc -ccc-install-dir %t/bin -nostdlib -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NOSTDLIB
// NOSTDLIB-NOT: crt0.rel
// NOSTDLIB-NOT: "-k"
// NOSTDLIB-NOT: "-l"

// Without SDCC's library, linking is an error.
// RUN: mkdir -p %t/none/bin && touch %t/none/bin/sdldz80 && chmod +x %t/none/bin/sdldz80
// RUN: not env -u SDCC_LIB -u SDCC_HOME PATH=%t/none/bin %clang --target=z80-unknown-none-sdcc --sysroot=%t/none -ccc-install-dir %t/none/bin -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=MISSING
// MISSING: error: cannot find SDCC's library 'z80.lib'; is SDCC installed?

// A Z80 library from before SDCC 4.2 is an error too.
// RUN: mkdir -p %t/old/z80 && touch %t/old/z80/z80.lib
// RUN: not env SDCC_LIB=%t/old PATH=%t/bin %clang --target=z80-unknown-none-sdcc -ccc-install-dir %t/bin -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=OLD
// OLD: error: the SDCC library in '{{.*}}/old/z80' is from SDCC before 4.2

// A missing runtime is an error too.
// RUN: mkdir -p %t/nort/bin
// RUN: not env SDCC_LIB=%t/env PATH=%t/bin %clang --target=z80-unknown-none-sdcc -ccc-install-dir %t/nort/bin -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NORT
// NORT: error: cannot find the runtime library 'z80_sdcc_rt.lib'
