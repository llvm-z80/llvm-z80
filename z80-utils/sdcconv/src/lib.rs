//! Convert objects between the Z80 ELF format Clang writes and the ASxxxx
//! `.rel` format the SDCC tools use, so code from either can be linked with
//! the other's linker.
//!
//! Which ELF sections become which SDCC areas is decided in `areas`, and how
//! relocations correspond in `relocs`; the conversions themselves only apply
//! those tables.

mod archive;
mod areas;
mod elf2rel;
mod rel2elf;
mod relocs;

pub use archive::{elf_archive_to_lib, lib_to_elf_archive};
pub use elf2rel::elf_to_rel;
pub use rel2elf::{rel_to_elf, rename_libc_symbols};

use std::fmt;

#[derive(Debug)]
pub struct Error(String);

impl Error {
    fn new(message: impl Into<String>) -> Error {
        Error(message.into())
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.0)
    }
}

impl std::error::Error for Error {}

impl From<asxxxx::Error> for Error {
    fn from(e: asxxxx::Error) -> Error {
        Error(e.to_string())
    }
}

impl From<object::read::Error> for Error {
    fn from(e: object::read::Error) -> Error {
        Error(e.to_string())
    }
}

impl From<object::write::Error> for Error {
    fn from(e: object::write::Error) -> Error {
        Error(e.to_string())
    }
}

impl From<std::io::Error> for Error {
    fn from(e: std::io::Error) -> Error {
        Error(e.to_string())
    }
}
