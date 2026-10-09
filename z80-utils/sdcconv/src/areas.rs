//! Which ELF section becomes which SDCC area, and back.
//!
//! The SDCC conventions followed are those of its crt0: `_DATA` is zeroed at
//! startup, and `_INITIALIZER` is copied over `_INITIALIZED`.

use object::elf::{SHF_ALLOC, SHF_EXECINSTR, SHF_WRITE, SHT_NOBITS, SHT_PROGBITS};

/// The areas SDCC's compilers write, in their order, with their flags.
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

/// The largest address space in the Z80 family, the eZ80's 24 bits.
pub const ADDRESS_SPACE: u32 = 0x100_0000;

pub const INITIALIZED: &str = "_INITIALIZED";
pub const INITIALIZER: &str = "_INITIALIZER";

/// Where an ELF section goes.
pub enum Place<'a> {
    Area(&'a str),
    /// Initialized writable data: its symbols in `_INITIALIZED`, its bytes in
    /// `_INITIALIZER`.
    Initialized,
}

pub fn area_for_section(name: &str, sh_type: u32, flags: u32) -> Place<'_> {
    match name {
        ".text.home" => return Place::Area("_HOME"),
        ".text.gsinit" => return Place::Area("_GSINIT"),
        ".text.gsfinal" => return Place::Area("_GSFINAL"),
        // An area rel2elf passed through under its own name. elf2rel refuses
        // a section named after one of SDCC's own areas.
        _ if name.starts_with('_') => return Place::Area(name),
        _ => {}
    }
    if sh_type == SHT_NOBITS {
        Place::Area("_DATA")
    } else if flags & SHF_EXECINSTR != 0 {
        Place::Area("_CODE")
    } else if flags & SHF_WRITE != 0 {
        Place::Initialized
    } else {
        Place::Area("_CODE")
    }
}

/// The section an area becomes, as name, type and flags. `empty` says the
/// area holds nothing but zeros, so it can take no file space.
pub fn section_for_area(area: &str, empty: bool) -> (String, u32, u32) {
    let code = SHF_ALLOC | SHF_EXECINSTR;
    let data = SHF_ALLOC | SHF_WRITE;
    let (name, sh_type, flags) = match area {
        "_CODE" => (".text", SHT_PROGBITS, code),
        "_HOME" => (".text.home", SHT_PROGBITS, code),
        "_GSINIT" => (".text.gsinit", SHT_PROGBITS, code),
        "_GSFINAL" => (".text.gsfinal", SHT_PROGBITS, code),
        "_CABS" => (".text.abs", SHT_PROGBITS, code),
        "_DABS" => (".data.abs", SHT_PROGBITS, data),
        INITIALIZED | INITIALIZER => (".data", SHT_PROGBITS, data),
        "_DATA" | "_BSS" if empty => (".bss", SHT_NOBITS, data),
        "_DATA" | "_BSS" => (".data", SHT_PROGBITS, data),
        // elf2rel passes a section back under its own name only when the
        // name starts with `_`.
        other if !other.starts_with('_') => return section_for_area(&format!("_{other}"), empty),
        other if empty => (other, SHT_NOBITS, data),
        other => (other, SHT_PROGBITS, SHF_ALLOC),
    };
    (name.to_string(), sh_type, flags)
}
