//! Read and write ASxxxx object files (`.rel`)

pub mod raw;
mod read;
mod write;

pub use raw::{Format, Radix};
pub use read::{Linker, parse, parse_as};
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
    /// Base page definitions (`P` lines) in file order, which page-relative
    /// relocations refer to.
    pub pages: Vec<PageBase>,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Area {
    pub name: String,
    pub flags: u32,
    pub addr: u32,
    pub size: u32,
    /// The bytes the linker places, sorted by offset and merged where they
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
    /// Offset in its area of the first byte the field becomes.
    pub offset: u32,
    pub field: Field,
    pub pc_relative: bool,
    /// The linker rejects a byte result that does not fit unsigned.
    pub unsigned: bool,
    pub page: Page,
    pub target: Target,
    /// The value the field held, sign-extended from its width.
    pub addend: i64,
}

/// What a relocation fills in.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Field {
    Word,
    /// A 24-bit word, such as a DS80C390 or STM8 far address.
    Word24,
    Byte,
    /// One byte of an address-wide value: 0 the low byte, 1 the next.
    AddrByte(u8),
    /// One byte of a 24-bit value: 0 low, 1 middle, 2 high. `bytx` marks the
    /// address-width flag written alongside, which the linker counts when it
    /// locates later fields on the same line.
    LongByte {
        index: u8,
        bytx: bool,
    },
    /// An 8051 bit address, made from the byte address and bit number in an
    /// address-wide value.
    AddrBit,
    /// The same from a 24-bit value.
    LongBit {
        bytx: bool,
    },
    /// An 8051 11-bit jump target (`ajmp`, `acall`), whose top bits the
    /// linker merges into `opcode`.
    Jump11 {
        opcode: u8,
    },
    /// A Padauk instruction with an address merged into its opcode.
    Pdk {
        part: PdkPart,
        opcode: u16,
        version: u8,
    },
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PdkPart {
    Whole,
    Low,
    High,
}

impl Field {
    /// Bytes the field becomes once linked.
    pub fn size(self) -> u32 {
        match self {
            Field::Word | Field::Jump11 { .. } | Field::Pdk { .. } => 2,
            Field::Word24 => 3,
            Field::Byte
            | Field::AddrByte(_)
            | Field::LongByte { .. }
            | Field::AddrBit
            | Field::LongBit { .. } => 1,
        }
    }
}

/// Which page a page-relative relocation is relative to. Each names the base
/// page definition in effect, an index into [`Object::pages`], or `None` for
/// the default one.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum Page {
    #[default]
    None,
    /// Page zero (`R_PAG0`).
    Zero(Option<usize>),
    /// The current page (`R_PAG`).
    Paged(Option<usize>),
}

/// A base page definition: the values of its `T` line, which hold the page
/// area's index and the page's base, and the relocations of those values.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PageBase {
    /// The area the `P` line names.
    pub area: usize,
    pub bytes: Vec<u8>,
    /// Each relocated value: its position in `bytes`, its mode bits and what
    /// it refers to.
    pub relocs: Vec<(u8, u16, Target)>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Target {
    Symbol(usize),
    Area(usize),
    /// Address zero, which sdas uses for a PC-relative reference to a fixed
    /// address.
    Absolute,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Error {
    /// The offending line of the input, or 0 when no one line is to blame,
    /// as for a problem in the model being written.
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

/// Relocation mode bits.
mod mode {
    pub const BYTE: u16 = 0x01;
    pub const SYM: u16 = 0x02;
    pub const PCR: u16 = 0x04;
    pub const BYTX: u16 = 0x08;
    pub const USGN: u16 = 0x10;
    pub const PAG0: u16 = 0x20;
    pub const PAG: u16 = 0x40;
    pub const MSB: u16 = 0x80;
    pub const BYT3: u16 = 0x100;
    pub const HIB: u16 = 0x200;
    pub const BIT: u16 = 0x400;
}

/// Most values one `T` line may hold, address included. The linker cannot
/// read a longer one.
const T_LINE_VALUES: usize = 16;

/// The symbol every module defines, which carries no information. No other
/// symbol may take its name.
pub const ABS_SYMBOL: &str = ".__.ABS.";

/// Whether `name` can name an area or a symbol, which the parser reads as one
/// word.
pub fn valid_name(name: &str) -> bool {
    !name.is_empty() && !name.contains(|c: char| c.is_whitespace() || c.is_control())
}
