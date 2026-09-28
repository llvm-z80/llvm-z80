//! Regression test for the elf2rel .bss-materialization bug (#253).
//!
//! `.bss` sections (ELF `SHT_NOBITS`) used to be routed to the SDCC `_DATA`
//! area, and their zero bytes were appended to that area's byte buffer --
//! inflating every emitted `.rel` file by the full size of each uninitialized
//! static.  Example: `char flags[8191]` in sieve.c inflated SIEVE.COM from
//! 188 B to 10179 B.
//!
//! Fix: `section_to_area()` now routes `.bss`/`.bss.*` to a dedicated `_BSS`
//! area.  For `SHT_NOBITS` sections only the area's logical size grows; no
//! bytes are ever appended to the byte buffer, so no `T` records are emitted
//! for it (matching SDCC's own `_BSS` convention: zeroed by the linker/crt0
//! at load time, not file-resident).
//!
//! The fixture ELF is built in memory using the `object` crate (no prebuilt
//! binary committed to the repo).  The object crate 0.36 has no
//! `Architecture::Z80`, so we build with `Architecture::I386` (same ELF32
//! class) and patch `e_machine` to `0x1F90` (the value emitted by
//! `clang --target=z80`) before passing the bytes to elf2rel.

use object::write::Object;
use object::{Architecture, BinaryFormat, Endianness, SectionKind};
use std::process::Command;

/// EM_Z80 as used by the ravn/llvm-z80 clang backend (also checked by elf2rel).
/// Little-endian bytes for ELF32 header field `e_machine` at offset 18.
const EM_Z80_LE: [u8; 2] = [0x90, 0x1F]; // 0x1F90
const ELF32_E_MACHINE_OFFSET: usize = 18;

/// Build a minimal Z80 ELF32 with:
/// - `.text` PROGBITS section (1 byte, so _CODE has content)
/// - `.bss`  NOBITS section  (4096 bytes, uninitialized)
fn build_bss_fixture_elf() -> Vec<u8> {
    let mut obj = Object::new(BinaryFormat::Elf, Architecture::I386, Endianness::Little);

    let text = obj.add_section(b"".to_vec(), b".text".to_vec(), SectionKind::Text);
    obj.append_section_data(text, &[0x00], 1);

    // 4096 B uninitialized -> .bss (SHT_NOBITS).
    // Before the fix: routed to _DATA, 4096 zero bytes written to .rel (bug).
    // After the fix:  routed to _BSS, no bytes written (correct).
    let bss = obj.add_section(b"".to_vec(), b".bss".to_vec(), SectionKind::UninitializedData);
    obj.append_section_bss(bss, 4096, 1);

    let mut bytes = obj.write().unwrap();
    bytes[ELF32_E_MACHINE_OFFSET..ELF32_E_MACHINE_OFFSET + 2].copy_from_slice(&EM_Z80_LE);
    bytes
}

#[test]
fn bss_static_does_not_inflate_rel_file() {
    // Write the in-memory fixture to a temp file so elf2rel can read it.
    let tmp_dir = std::env::temp_dir();
    let pid = std::process::id();
    let in_path = tmp_dir.join(format!("elf2rel_bss_in_{pid}.o"));
    let out_path = tmp_dir.join(format!("elf2rel_bss_out_{pid}.rel"));

    std::fs::write(&in_path, build_bss_fixture_elf()).expect("write fixture");

    let status = Command::new(env!("CARGO_BIN_EXE_elf2rel"))
        .arg(&in_path)
        .arg(&out_path)
        .status()
        .expect("run elf2rel");
    let _ = std::fs::remove_file(&in_path);
    assert!(status.success(), "elf2rel exited with failure");

    let rel = std::fs::read_to_string(&out_path).expect("read .rel output");
    let rel_size = rel.len();
    let _ = std::fs::remove_file(&out_path);

    // With only .bss content, _DATA must be absent entirely
    // (empty areas are filtered out by the active_areas logic in main.rs).
    assert!(
        !rel.lines().any(|l| l.starts_with("A _DATA")),
        "did not expect a `_DATA` area: buf is .bss, not real data\n{rel}"
    );

    // buf must land in _BSS sized exactly 0x1000 (4096 = sizeof(buf)).
    let bss_line = rel
        .lines()
        .find(|l| l.starts_with("A _BSS"))
        .expect("expected an `A _BSS ...` area header line");
    assert!(
        bss_line.contains("size 1000 "),
        "expected `_BSS` sized 0x1000, got: {bss_line:?}"
    );

    // No T-records after _BSS -- BSS must not be file-resident.
    let after_bss = &rel[rel.find("A _BSS").unwrap()..];
    assert!(
        !after_bss.lines().skip(1).any(|l| l.starts_with('T')),
        "no T records expected after _BSS header\n{after_bss}"
    );

    // Before the fix this was 22096 B (4096 zero bytes + T-record overhead).
    // After the fix it is ~200 B.  1024 B is a safe upper bound.
    assert!(
        rel_size < 1024,
        "expected small .rel (no materialized .bss); got {rel_size} B (was 22096 B with the bug)"
    );
}
