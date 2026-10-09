use std::io::Cursor;
use std::path::Path;

use ar_archive_writer::{
    ArchiveKind, DEFAULT_OBJECT_READER, NewArchiveMember, ObjectReader, write_archive_to_stream,
};
use object::read::archive::ArchiveFile;

use crate::{Error, elf_to_rel, rel_to_elf, rename_libc_symbols};

/// Lists the symbols a `.rel` member defines, for the archive's index.
static REL_READER: ObjectReader = ObjectReader {
    get_symbols: rel_symbols,
    ..DEFAULT_OBJECT_READER
};

fn rel_symbols(
    buf: &[u8],
    f: &mut dyn FnMut(&[u8]) -> std::io::Result<()>,
) -> std::io::Result<bool> {
    let Some(obj) = std::str::from_utf8(buf)
        .ok()
        .and_then(|t| asxxxx::parse(t).ok())
    else {
        return Ok(false);
    };
    for symbol in obj.symbols.iter().filter(|s| s.def.is_some()) {
        f(symbol.name.as_bytes())?;
    }
    Ok(true)
}

/// Convert an archive of ELF objects to an SDCC library of `.rel` objects,
/// also returning warnings. A member that cannot be converted is an error.
pub fn elf_archive_to_lib(data: &[u8]) -> Result<(Vec<u8>, Vec<String>), Error> {
    convert_members(data, "rel", &REL_READER, |name, data| {
        let module = Path::new(name)
            .file_stem()
            .unwrap_or_default()
            .to_string_lossy();
        let (obj, warnings) = elf_to_rel(data, &module)?;
        Ok((asxxxx::write(&obj)?.into_bytes(), warnings))
    })
}

/// Convert an SDCC library of `.rel` objects to an archive of ELF objects,
/// renaming clashing C library functions if asked to, and also returning
/// warnings. A member that cannot be converted is an error.
pub fn lib_to_elf_archive(data: &[u8], rename_libc: bool) -> Result<(Vec<u8>, Vec<String>), Error> {
    convert_members(data, "o", &DEFAULT_OBJECT_READER, |_, data| {
        let text = std::str::from_utf8(data).map_err(|_| Error::new("not a text object"))?;
        let mut obj = asxxxx::parse(text)?;
        if rename_libc {
            rename_libc_symbols(&mut obj);
        }
        Ok((rel_to_elf(&obj)?, Vec::new()))
    })
}

fn convert_members(
    data: &[u8],
    extension: &str,
    reader: &'static ObjectReader,
    convert: impl Fn(&str, &[u8]) -> Result<(Vec<u8>, Vec<String>), Error>,
) -> Result<(Vec<u8>, Vec<String>), Error> {
    let archive = ArchiveFile::parse(data)?;
    let mut members = Vec::new();
    let mut warnings = Vec::new();
    for member in archive.members() {
        let member = member?;
        let name = String::from_utf8_lossy(member.name()).into_owned();
        let (converted, member_warnings) =
            convert(&name, member.data(data)?).map_err(|e| Error::new(format!("{name}: {e}")))?;
        warnings.extend(member_warnings.iter().map(|w| format!("{name}: {w}")));
        members.push(NewArchiveMember {
            buf: Box::new(converted),
            object_reader: reader,
            member_name: Path::new(&name)
                .with_extension(extension)
                .file_name()
                .unwrap_or_default()
                .to_string_lossy()
                .into_owned(),
            mtime: 0,
            uid: 0,
            gid: 0,
            perms: 0o644,
        });
    }
    let mut out = Cursor::new(Vec::new());
    write_archive_to_stream(&mut out, &members, ArchiveKind::Gnu, false, None)?;
    Ok((out.into_inner(), warnings))
}
