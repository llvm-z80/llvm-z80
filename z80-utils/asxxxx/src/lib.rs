//! Read and write ASxxxx object files (`.rel`), the object format of the SDCC
//! assemblers and linker.
//!
//! The model covers what the Z80-family assemblers write: version 3 objects
//! with word, byte and address-byte relocations, optionally PC-relative.
//! Relocation fields are decoded into a value carried by the relocation, so
//! the area contents hold zeros under them.

mod read;
mod write;

pub use read::parse;
pub use write::write;

use std::fmt;

/// One object module.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Object {
    pub format: Format,
    pub name: Option<String>,
    /// `O` lines, which SDCC uses to record the options a module was built
    /// with.
    pub options: Vec<String>,
    pub areas: Vec<Area>,
    pub symbols: Vec<Symbol>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Format {
    pub big_endian: bool,
    /// Width of an address, which is also the width of an address-byte field
    /// before the linker narrows it.
    pub addr_bytes: u8,
}

impl Default for Format {
    /// What sdasz80 and sdasgb write.
    fn default() -> Self {
        Format {
            big_endian: false,
            addr_bytes: 4,
        }
    }
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Area {
    pub name: String,
    pub flags: u32,
    pub addr: u32,
    pub size: u32,
    /// The bytes `T` lines place, sorted by offset and merged where they
    /// touch.
    pub data: Vec<Chunk>,
    pub relocs: Vec<Reloc>,
}

impl Area {
    pub const ABS: u32 = 0x08;

    pub fn is_absolute(&self) -> bool {
        self.flags & Self::ABS != 0
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Chunk {
    pub offset: u32,
    pub bytes: Vec<u8>,
}

impl Chunk {
    pub fn end(&self) -> u32 {
        self.offset + self.bytes.len() as u32
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Symbol {
    pub name: String,
    /// `None` for a reference.
    pub def: Option<Def>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Def {
    /// `None` for a symbol defined before the first area, which is absolute.
    pub area: Option<usize>,
    pub value: u32,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Reloc {
    /// Offset of the field in its area.
    pub offset: u32,
    pub width: Width,
    pub pc_relative: bool,
    pub target: Target,
    /// The value the field held, sign-extended from its width.
    pub addend: i64,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Width {
    Word,
    Byte,
    /// The low byte of an address-wide value (`R_BYTX`).
    AddrLow,
    /// The high byte of an address-wide value (`R_BYTX | R_MSB`).
    AddrHigh,
}

impl Width {
    /// Bytes the field occupies once linked.
    pub fn size(self) -> u32 {
        match self {
            Width::Word => 2,
            Width::Byte | Width::AddrLow | Width::AddrHigh => 1,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Target {
    Symbol(usize),
    Area(usize),
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Error {
    /// The offending line of the input, or 0 for a problem in the model being
    /// written.
    pub line: usize,
    pub message: String,
}

impl Error {
    fn new(line: usize, message: impl Into<String>) -> Error {
        Error {
            line,
            message: message.into(),
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.line == 0 {
            f.write_str(&self.message)
        } else {
            write!(f, "line {}: {}", self.line, self.message)
        }
    }
}

impl std::error::Error for Error {}

const R_BYTE: u8 = 0x01;
const R_SYM: u8 = 0x02;
const R_PCR: u8 = 0x04;
const R_BYTX: u8 = 0x08;
const R_MSB: u8 = 0x80;

/// The symbol every module defines first, which carries no information.
const ABS_SYMBOL: &str = ".__.ABS.";
