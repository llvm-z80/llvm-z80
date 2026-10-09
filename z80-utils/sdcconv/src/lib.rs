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

#[derive(Debug, thiserror::Error)]
pub enum Error {
    /// The object holds something the other format cannot.
    #[error("{0}")]
    Convert(String),
    #[error(transparent)]
    Rel(#[from] asxxxx::Error),
    #[error(transparent)]
    ReadElf(#[from] object::read::Error),
    #[error(transparent)]
    WriteElf(#[from] object::write::Error),
    #[error(transparent)]
    Io(#[from] std::io::Error),
}

impl Error {
    fn new(message: impl Into<String>) -> Error {
        Error::Convert(message.into())
    }
}
