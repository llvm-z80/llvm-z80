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
const R_Z80_IMM16: u32 = 16;
const R_SM83_LDH8: u32 = 19;

pub struct Row {
    pub r_type: u32,
    pub width: Width,
    pub pc_relative: bool,
    /// The target lies in SM83 high RAM, which `ldh` reaches.
    pub hram: bool,
    /// What the ASxxxx field holds beyond the ELF addend. sdld measures a
    /// PC-relative value from the end of the field, lld from wherever its
    /// relocation type says.
    pub pc_bias: i64,
}

const fn row(r_type: u32, width: Width) -> Row {
    Row {
        r_type,
        width,
        pc_relative: false,
        hram: false,
        pc_bias: 0,
    }
}

/// When several rows fit an ASxxxx relocation, rel2elf takes the first.
const TABLE: &[Row] = &[
    row(R_Z80_ADDR16, Width::Word),
    row(R_Z80_IMM16, Width::Word),
    row(R_Z80_ADDR8, Width::Byte),
    row(R_Z80_IMM8, Width::Byte),
    Row {
        hram: true,
        ..row(R_SM83_LDH8, Width::Byte)
    },
    row(R_Z80_ADDR16_LO, Width::AddrLow),
    row(R_Z80_ADDR16_HI, Width::AddrHigh),
    Row {
        pc_relative: true,
        ..row(R_Z80_PCREL_8, Width::Byte)
    },
    Row {
        pc_relative: true,
        pc_bias: 1,
        ..row(R_Z80_PCREL_16, Width::Word)
    },
];

pub enum FromElf {
    Field(&'static Row),
    Ignore,
    Unsupported,
}

pub fn from_elf(r_type: u32) -> FromElf {
    if r_type == R_Z80_NONE {
        return FromElf::Ignore;
    }
    match TABLE.iter().find(|r| r.r_type == r_type) {
        Some(row) => FromElf::Field(row),
        None => FromElf::Unsupported,
    }
}

pub fn to_elf(width: Width, pc_relative: bool, hram: bool) -> Option<&'static Row> {
    // sdas writes a jr target as the low byte of an address-wide field.
    let width = match width {
        Width::AddrLow if pc_relative => Width::Byte,
        width => width,
    };
    let fits = |r: &&Row| r.width == width && r.pc_relative == pc_relative;
    TABLE
        .iter()
        .filter(fits)
        .find(|r| r.hram == hram)
        .or_else(|| TABLE.iter().filter(fits).find(|r| !r.hram))
}
