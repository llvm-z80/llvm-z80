//! Regression test for https://github.com/llvm-z80/llvm-z80/issues/253:
//! `.bss` sections are routed into the SDCC `_DATA` area by
//! `section_to_area()`, and because they are `SHT_NOBITS`, their zero bytes
//! are materialized directly into that area's byte buffer -- inflating the
//! emitted `.rel` file by the full size of every uninitialized static, even
//! though nothing is actually initialized.
//!
//! Fixture: `tests/fixtures/bss_repro.o`, built from
//! `tests/fixtures/bss_repro.c` (a single 4096-byte uninitialized global,
//! `char buf[4096]`, plus one trivial function) via:
//!   clang --target=z80 -Os -ffreestanding -ffunction-sections \
//!     -fdata-sections -c bss_repro.c -o bss_repro.o
//! Confirmed via `llvm-objdump -h bss_repro.o` that `buf` lands in a real
//! `.bss._buf` section of type `BSS` (SHT_NOBITS), size 0x1000, as expected.
//!
//! Current (buggy) behavior: elf2rel emits an `_DATA` area sized 0x1000
//! with 4096 literal zero bytes written to the `.rel` file (668 B input ->
//! 22+ KB output). Expected behavior once #253 is fixed: `buf` should land
//! in a dedicated, non-file-resident `_BSS` area (SDCC convention -- present
//! in the area/size table for layout purposes, but contributing no bytes to
//! the file), and `_DATA` should stay empty.
//!
//! Marked `#[ignore]` until the fix lands -- run explicitly with
//! `cargo test -- --ignored` to observe the current failure. Remove the
//! `#[ignore]` attribute (and this comment) once #253 is closed.

use std::process::Command;

#[ignore = "known bug llvm-z80/llvm-z80#253: elf2rel routes .bss into _DATA \
            and materializes its zero bytes into the .rel file; remove \
            #[ignore] once elf2rel emits a real, byte-free _BSS area"]
#[test]
fn bss_only_static_does_not_inflate_rel_file() {
    let fixture = concat!(env!("CARGO_MANIFEST_DIR"), "/tests/fixtures/bss_repro.o");
    let out_path = std::env::temp_dir().join(format!(
        "elf2rel_bss_area_test_{}.rel",
        std::process::id()
    ));

    let status = Command::new(env!("CARGO_BIN_EXE_elf2rel"))
        .arg(fixture)
        .arg(&out_path)
        .status()
        .expect("failed to run elf2rel");
    assert!(status.success(), "elf2rel exited with failure");

    let rel_text = std::fs::read_to_string(&out_path).expect("failed to read .rel output");
    let _ = std::fs::remove_file(&out_path);

    // `buf` (4096 B, uninitialized) must not show up as _DATA content: a
    // correct converter either omits _DATA entirely or emits it with size 0,
    // routing `buf` into a real _BSS area instead.
    let data_area_line = rel_text
        .lines()
        .find(|l| l.starts_with("A _DATA"))
        .expect("expected an `A _DATA ...` area header line in the .rel output");
    assert!(
        data_area_line.contains("size 0 ") || data_area_line.contains("size 0\n"),
        "expected `_DATA` area to be empty (buf is .bss, not real data), \
         but got: {data_area_line:?} -- the 4096-byte uninitialized static \
         is being materialized as literal zero bytes in _DATA (issue #253)"
    );

    // A real fix should introduce a dedicated _BSS area for uninitialized
    // statics rather than silently dropping them.
    assert!(
        rel_text.contains("_BSS"),
        "expected a dedicated `_BSS` area for the uninitialized `buf` static, \
         found none in the .rel output (issue #253)"
    );
}
