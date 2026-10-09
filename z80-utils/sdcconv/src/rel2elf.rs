use asxxxx::{Object, Target, Width};
use object::elf;
use object::write::StringId;
use object::write::elf::{FileHeader, Rel, SectionHeader, SectionIndex, Sym, Writer};

use crate::Error;
use crate::areas::{INITIALIZED, INITIALIZER, section_for_area};
use crate::relocs::{EM_Z80, to_elf};

/// Symbols from SDCC libraries that clash with Rust's compiler_builtins,
/// renamed with a `__sdcc` suffix so SDCC code keeps its own versions.
const CONFLICTING_SYMBOLS: &[&str] = &["_memcpy", "_memmove", "_memset", "_strlen", "_strcmp"];

struct Section {
    area: usize,
    name: String,
    nobits: bool,
    flags: u32,
    addr: u32,
    data: Vec<u8>,
    relocs: Vec<Rel>,
}

/// Convert an ASxxxx object to a Z80 ELF relocatable object.
pub fn rel_to_elf(obj: &Object) -> Result<Vec<u8>, Error> {
    // An absolute area places its contents and symbols at their addresses;
    // the section starts at the area's address instead.
    let base = |a: usize| {
        let area = &obj.areas[a];
        if area.is_absolute() { area.addr } else { 0 }
    };

    let find = |name| obj.areas.iter().position(|a| a.name == name);
    let initialized = find(INITIALIZED);
    let initializer = find(INITIALIZER);
    if let Some(i) = initialized
        && (!obj.areas[i].data.is_empty() || !obj.areas[i].relocs.is_empty())
    {
        return Err(Error::new(format!("{INITIALIZED} has contents of its own")));
    }
    let size = |a: Option<usize>| a.map_or(0, |a| obj.areas[a].size);
    if size(initialized) != size(initializer) {
        return Err(Error::new(format!(
            "{INITIALIZED} and {INITIALIZER} differ in size"
        )));
    }

    let mut sections: Vec<Section> = Vec::new();
    let mut section_of = vec![None; obj.areas.len()];
    for (a, area) in obj.areas.iter().enumerate() {
        // Initialized data is one section, holding the bytes of
        // `_INITIALIZER` and the symbols of `_INITIALIZED`.
        let paired = [initialized, initializer].contains(&Some(a));
        let contents = if paired { initializer } else { Some(a) };
        if paired && section_of[a].is_some() {
            continue;
        }
        let Some(contents) = contents else {
            continue;
        };
        let source = &obj.areas[contents];
        let data = layout(source, base(contents));
        if data.is_empty() && source.relocs.is_empty() && source.size == 0 {
            continue;
        }
        let empty = source.relocs.is_empty() && data.iter().all(|&b| b == 0);
        let (name, sh_type, flags) = section_for_area(&area.name, empty);
        let index = Some(sections.len());
        section_of[a] = index;
        if paired {
            section_of[initialized.unwrap_or(a)] = index;
            section_of[contents] = index;
        }
        sections.push(Section {
            area: contents,
            name: name.to_string(),
            nobits: sh_type == elf::SHT_NOBITS,
            flags,
            addr: if source.is_absolute() { source.addr } else { 0 },
            data,
            relocs: Vec::new(),
        });
    }

    // Symbol table: null, one per section, then the globals in file order.
    let section_count = sections.len();
    let section_symbol = |s: usize| (1 + s) as u32;
    let global_symbol = |i: usize| (1 + section_count + i) as u32;

    for section in &mut sections {
        let area = &obj.areas[section.area];
        let start = base(section.area);
        for reloc in &area.relocs {
            let hram = match reloc.target {
                Target::Symbol(s) => obj.symbols[s]
                    .def
                    .and_then(|d| d.area)
                    .is_some_and(|a| obj.areas[a].name == "_HRAM"),
                Target::Area(a) => obj.areas[a].name == "_HRAM",
            };
            let (r_sym, r_addend) = match reloc.target {
                Target::Symbol(s) => (global_symbol(s), reloc.addend),
                Target::Area(a) => {
                    let sym = section_of[a].map_or(0, section_symbol);
                    (sym, area_addend(reloc.width, reloc.addend, base(a)))
                }
            };
            let row = to_elf(reloc.width, reloc.pc_relative, hram).ok_or_else(|| {
                Error::new(format!(
                    "unsupported relocation at {:#06X} in {}",
                    reloc.offset, area.name
                ))
            })?;
            section.relocs.push(Rel {
                r_offset: u64::from(reloc.offset - start),
                r_sym,
                r_type: row.r_type,
                r_addend: i64::from((r_addend - row.pc_bias) as i32),
            });
            let at = (reloc.offset - start) as usize;
            let end = (at + reloc.width.size() as usize).min(section.data.len());
            if at < end {
                section.data[at..end].fill(0);
            }
        }
    }

    write_elf(obj, &sections, &section_of, base)
}

/// An area's contents as the linker lays them out, starting at `base`.
fn layout(area: &asxxxx::Area, base: u32) -> Vec<u8> {
    let mut data = vec![0; area.size as usize];
    for chunk in &area.data {
        let end = chunk.end() as usize;
        if end > data.len() {
            data.resize(end, 0);
        }
        data[chunk.offset as usize..end].copy_from_slice(&chunk.bytes);
    }
    let start = base as usize;
    if start == 0 {
        return data;
    }
    let end = (start + area.size as usize).min(data.len());
    data.get(start..end).map_or(Vec::new(), <[u8]>::to_vec)
}

/// The addend of a reference to an area: an absolute area's references hold
/// absolute addresses, which become offsets from its start.
fn area_addend(width: Width, value: i64, base: u32) -> i64 {
    let base = i64::from(base);
    match width {
        _ if base == 0 => value,
        Width::AddrLow | Width::AddrHigh => (value as u32 as i64) - base,
        Width::Byte => i64::from((base as u32 & 0xFF00) | u32::from(value as u8)) - base,
        Width::Word => i64::from(value as u16) - base,
    }
}

fn mangle(name: &str) -> String {
    if CONFLICTING_SYMBOLS.contains(&name) {
        format!("{name}__sdcc")
    } else {
        name.to_string()
    }
}

fn write_elf(
    obj: &Object,
    sections: &[Section],
    section_of: &[Option<usize>],
    base: impl Fn(usize) -> u32,
) -> Result<Vec<u8>, Error> {
    let mut buffer = Vec::new();
    let mut w = Writer::new(object::Endianness::Little, false, &mut buffer);

    w.reserve_file_header();
    w.reserve_null_section_index();
    let content: Vec<(SectionIndex, StringId)> = sections
        .iter()
        .map(|s| {
            (
                w.reserve_section_index(),
                w.add_section_name(s.name.as_bytes()),
            )
        })
        .collect();
    let rela_names: Vec<String> = sections
        .iter()
        .filter(|s| !s.relocs.is_empty())
        .map(|s| format!(".rela{}", s.name))
        .collect();
    let rela: Vec<(SectionIndex, StringId)> = rela_names
        .iter()
        .map(|n| (w.reserve_section_index(), w.add_section_name(n.as_bytes())))
        .collect();
    w.reserve_symtab_section_index();
    w.reserve_strtab_section_index();
    w.reserve_shstrtab_section_index();

    w.reserve_null_symbol_index();
    for &(index, _) in &content {
        w.reserve_symbol_index(Some(index));
    }
    let names: Vec<String> = obj.symbols.iter().map(|s| mangle(&s.name)).collect();
    let name_ids: Vec<StringId> = names.iter().map(|n| w.add_string(n.as_bytes())).collect();
    for _ in &names {
        w.reserve_symbol_index(None);
    }

    let offsets: Vec<usize> = sections
        .iter()
        .map(|s| {
            if s.nobits {
                w.reserved_len()
            } else {
                w.reserve(s.data.len(), 1)
            }
        })
        .collect();
    w.reserve_symtab();
    w.reserve_strtab();
    w.reserve_shstrtab();
    let rela_offsets: Vec<usize> = sections
        .iter()
        .filter(|s| !s.relocs.is_empty())
        .map(|s| w.reserve_relocations(s.relocs.len(), true))
        .collect();
    w.reserve_section_headers();

    w.write_file_header(&FileHeader {
        os_abi: 0,
        abi_version: 0,
        e_type: elf::ET_REL,
        e_machine: EM_Z80,
        e_entry: 0,
        e_flags: 0,
    })?;
    for section in sections.iter().filter(|s| !s.nobits) {
        w.write(&section.data);
    }

    w.write_null_symbol();
    for &(index, _) in &content {
        w.write_symbol(&Sym {
            name: None,
            section: Some(index),
            st_info: (elf::STB_LOCAL << 4) | elf::STT_SECTION,
            st_other: 0,
            st_shndx: 0,
            st_value: 0,
            st_size: 0,
        });
    }
    for (symbol, &name) in obj.symbols.iter().zip(&name_ids) {
        let (section, st_shndx, st_value) = match symbol.def {
            None => (None, elf::SHN_UNDEF, 0),
            Some(def) => match def.area.and_then(|a| section_of[a].map(|s| (a, s))) {
                Some((a, s)) => (Some(content[s].0), 0, def.value - base(a)),
                None => (None, elf::SHN_ABS, def.value),
            },
        };
        w.write_symbol(&Sym {
            name: Some(name),
            section,
            st_info: (elf::STB_GLOBAL << 4) | elf::STT_NOTYPE,
            st_other: 0,
            st_shndx,
            st_value: u64::from(st_value),
            st_size: 0,
        });
    }
    w.write_strtab();
    w.write_shstrtab();
    for section in sections.iter().filter(|s| !s.relocs.is_empty()) {
        w.write_align_relocation();
        for rel in &section.relocs {
            w.write_relocation(true, rel);
        }
    }

    w.write_null_section_header();
    for ((section, &(_, name)), &offset) in sections.iter().zip(&content).zip(&offsets) {
        w.write_section_header(&SectionHeader {
            name: Some(name),
            sh_type: if section.nobits {
                elf::SHT_NOBITS
            } else {
                elf::SHT_PROGBITS
            },
            sh_flags: u64::from(section.flags),
            sh_addr: u64::from(section.addr),
            sh_offset: offset as u64,
            sh_size: section.data.len() as u64,
            sh_link: 0,
            sh_info: 0,
            sh_addralign: 1,
            sh_entsize: 0,
        });
    }
    let symtab = w.symtab_index();
    let with_relocs = sections
        .iter()
        .enumerate()
        .filter(|(_, s)| !s.relocs.is_empty());
    for (((i, section), &(_, name)), &offset) in with_relocs.zip(&rela).zip(&rela_offsets) {
        w.write_relocation_section_header(
            name,
            content[i].0,
            symtab,
            offset,
            section.relocs.len(),
            true,
        );
    }
    w.write_symtab_section_header(1 + sections.len() as u32);
    w.write_strtab_section_header();
    w.write_shstrtab_section_header();

    Ok(buffer)
}
