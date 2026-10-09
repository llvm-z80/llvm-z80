//! Which ELF section becomes which SDCC area, and back.

use object::elf::{SHF_ALLOC, SHF_EXECINSTR, SHF_WRITE};

/// The areas an object converted from ELF can have, in the order SDCC's
/// compilers list them, with their flags.
pub const AREAS: &[(&str, u32)] = &[
    ("_CODE", 0),
    ("_DATA", 0),
    ("_BSS", 0),
    ("_INITIALIZED", 0),
    ("_DABS", asxxxx::Area::ABS),
    ("_HOME", 0),
    ("_GSINIT", 0),
    ("_GSFINAL", 0),
    ("_INITIALIZER", 0),
    ("_CABS", asxxxx::Area::ABS),
];

pub fn area_for_section(name: &str, nobits: bool) -> &'static str {
    if nobits {
        "_BSS"
    } else if name == ".data" || name.starts_with(".data.") {
        "_DATA"
    } else {
        "_CODE"
    }
}

/// The section an area becomes and its flags.
pub fn section_for_area(area: &str) -> (&str, u32) {
    let code = SHF_ALLOC | SHF_EXECINSTR;
    let data = SHF_ALLOC | SHF_WRITE;
    match area {
        "_CODE" => (".text", code),
        "_HOME" => (".text.home", code),
        "_GSINIT" => (".text.gsinit", code),
        "_GSFINAL" => (".text.gsfinal", code),
        "_CABS" => (".text.abs", code),
        "_DATA" => (".data", data),
        "_BSS" => (".bss", data),
        "_INITIALIZED" => (".data.initialized", data),
        "_DABS" => (".data.abs", data),
        "_INITIALIZER" => (".rodata", SHF_ALLOC),
        other => (other, SHF_ALLOC),
    }
}

/// Whether an area holding only zeros becomes a NOBITS section.
pub fn may_be_bss(area: &str) -> bool {
    matches!(area, "_DATA" | "_BSS")
}
