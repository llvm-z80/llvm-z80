//! How Z80 ELF relocations correspond to ASxxxx ones.

use asxxxx::{Field, Page, Reloc};

pub const EM_Z80: u16 = 0x1F90;

const R_Z80_NONE: u32 = 0;
const R_Z80_IMM8: u32 = 1;
const R_Z80_ADDR8: u32 = 2;
const R_Z80_ADDR16: u32 = 3;
const R_Z80_ADDR16_LO: u32 = 4;
const R_Z80_ADDR16_HI: u32 = 5;
const R_Z80_PCREL_8: u32 = 6;
const R_Z80_PCREL_16: u32 = 12;
const R_Z80_FK_DATA_4: u32 = 13;
const R_Z80_FK_DATA_8: u32 = 14;
const R_Z80_IMM16: u32 = 16;
const R_SM83_LDH8: u32 = 19;

pub enum FromElf {
    Field {
        field: Field,
        pc_relative: bool,
    },
    /// Carries nothing the linker needs.
    Ignore,
    Unsupported,
}

pub fn from_elf(r_type: u32) -> FromElf {
    let field = |field, pc_relative| FromElf::Field { field, pc_relative };
    match r_type {
        R_Z80_NONE | R_Z80_FK_DATA_4 | R_Z80_FK_DATA_8 => FromElf::Ignore,
        R_Z80_ADDR16 | R_Z80_IMM16 => field(Field::Word, false),
        R_Z80_ADDR8 | R_Z80_IMM8 | R_SM83_LDH8 | R_Z80_ADDR16_LO | R_Z80_ADDR16_HI => {
            field(Field::Byte, false)
        }
        R_Z80_PCREL_8 => field(Field::Byte, true),
        R_Z80_PCREL_16 => field(Field::Word, true),
        _ => FromElf::Unsupported,
    }
}

/// The relocation type for `reloc`, if it is one the Z80 family uses. `hram`
/// says the target lies in SM83 high RAM, which `ldh` reaches.
pub fn to_elf(reloc: &Reloc, hram: bool) -> Option<u32> {
    if reloc.unsigned || reloc.page != Page::None {
        return None;
    }
    Some(match (reloc.field, reloc.pc_relative) {
        (Field::AddrByte(0), _) => R_Z80_ADDR16_LO,
        (Field::AddrByte(1), _) => R_Z80_ADDR16_HI,
        (Field::Byte, true) => R_Z80_PCREL_8,
        (Field::Byte, false) if hram => R_SM83_LDH8,
        (Field::Byte, false) => R_Z80_ADDR8,
        (Field::Word, true) => R_Z80_PCREL_16,
        (Field::Word, false) => R_Z80_ADDR16,
        _ => return None,
    })
}
