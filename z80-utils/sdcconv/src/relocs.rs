//! How Z80 ELF relocations correspond to ASxxxx ones.

use asxxxx::Width;

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
        width: Width,
        pc_relative: bool,
    },
    /// Carries nothing the linker needs.
    Ignore,
    Unsupported,
}

pub fn from_elf(r_type: u32) -> FromElf {
    let field = |width, pc_relative| FromElf::Field { width, pc_relative };
    match r_type {
        R_Z80_NONE | R_Z80_FK_DATA_4 | R_Z80_FK_DATA_8 => FromElf::Ignore,
        R_Z80_ADDR16 | R_Z80_IMM16 => field(Width::Word, false),
        R_Z80_ADDR8 | R_Z80_IMM8 | R_SM83_LDH8 | R_Z80_ADDR16_LO | R_Z80_ADDR16_HI => {
            field(Width::Byte, false)
        }
        R_Z80_PCREL_8 => field(Width::Byte, true),
        R_Z80_PCREL_16 => field(Width::Word, true),
        _ => FromElf::Unsupported,
    }
}

/// `hram` says the target lies in SM83 high RAM, which `ldh` reaches.
pub fn to_elf(width: Width, pc_relative: bool, hram: bool) -> u32 {
    match (width, pc_relative) {
        (Width::AddrLow, _) => R_Z80_ADDR16_LO,
        (Width::AddrHigh, _) => R_Z80_ADDR16_HI,
        (Width::Byte, true) => R_Z80_PCREL_8,
        (Width::Byte, false) if hram => R_SM83_LDH8,
        (Width::Byte, false) => R_Z80_ADDR8,
        (Width::Word, true) => R_Z80_PCREL_16,
        (Width::Word, false) => R_Z80_ADDR16,
    }
}
