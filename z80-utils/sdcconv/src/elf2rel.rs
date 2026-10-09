use std::collections::HashMap;

use asxxxx::{Area, Chunk, Def, Field, Object, Page, Reloc, Symbol, Target};
use object::LittleEndian as LE;
use object::elf::{self, FileHeader32, Rela32};
use object::read::elf::{FileHeader, Rela, SectionHeader, Sym};

use crate::Error;
use crate::areas::{ADDRESS_SPACE, AREAS, INITIALIZED, INITIALIZER, Place, area_for_section};
use crate::relocs::{EM_Z80, FromElf, from_elf};

/// Convert a Z80 ELF relocatable object to an ASxxxx object named after
/// `module`, with the characters a module name cannot hold replaced by `_`.
/// Also returns warnings about what the object could not keep.
pub fn elf_to_rel(data: &[u8], module: &str) -> Result<(Object, Vec<String>), Error> {
    let header = FileHeader32::<LE>::parse(data)?;
    let endian = header.endian()?;
    let machine = header.e_machine(endian);
    if machine != EM_Z80 {
        return Err(Error::new(format!(
            "not a Z80 ELF (e_machine={machine:#06X}, expected {EM_Z80:#06X})"
        )));
    }
    let sections = header.sections(endian, data)?;
    let symbols = sections.symbols(endian, data, elf::SHT_SYMTAB)?;

    let mut areas: Vec<Area> = AREAS
        .iter()
        .map(|&(name, flags)| Area {
            name: name.to_string(),
            flags,
            ..Area::default()
        })
        .collect();

    let mut placed: HashMap<usize, Placed> = HashMap::new();
    for (i, section) in sections.iter().enumerate() {
        let sh_type = section.sh_type(endian);
        let flags = section.sh_flags(endian);
        if (sh_type != elf::SHT_PROGBITS && sh_type != elf::SHT_NOBITS)
            || flags & elf::SHF_ALLOC == 0
        {
            continue;
        }
        let name = std::str::from_utf8(sections.section_name(endian, section)?).unwrap_or("");
        if name.is_empty() || name.starts_with(".note") || name.starts_with(".comment") {
            continue;
        }
        // Their contents would land where SDCC's own conventions for those
        // areas do not expect them.
        if AREAS.iter().any(|&(area, _)| area == name) {
            return Err(Error::new(format!(
                "section {name:?} takes the name of an area SDCC reserves"
            )));
        }
        let (area, contents) = match area_for_section(name, sh_type, flags) {
            Place::Area(a) => {
                if !asxxxx::valid_area_name(a) {
                    return Err(Error::new(format!(
                        "section {a:?} cannot be named in a .rel object"
                    )));
                }
                let a = area_index(&mut areas, a);
                (a, a)
            }
            Place::Initialized => (
                area_index(&mut areas, INITIALIZED),
                area_index(&mut areas, INITIALIZER),
            ),
        };
        let offset = areas[area].size;
        let size = section.sh_size(endian);
        if u64::from(offset) + u64::from(size) > u64::from(ADDRESS_SPACE) {
            return Err(Error::new(format!(
                "{name} makes {} larger than any Z80 address space",
                areas[area].name
            )));
        }
        let data_size = if sh_type == elf::SHT_PROGBITS {
            size
        } else {
            0
        };
        placed.insert(
            i,
            Placed {
                area,
                contents,
                offset,
                data_size,
            },
        );
        if sh_type == elf::SHT_PROGBITS {
            let bytes = section.data(endian, data)?;
            let contents = &mut areas[contents];
            match contents.data.last_mut() {
                Some(chunk) if chunk.end() == offset => chunk.bytes.extend_from_slice(bytes),
                _ => contents.data.push(Chunk {
                    offset,
                    bytes: bytes.to_vec(),
                }),
            }
        }
        areas[area].size += size;
        if contents != area {
            areas[contents].size += size;
        }
    }

    let mut warnings = Vec::new();
    let mut model_symbols = Vec::new();
    let mut symbol_index: HashMap<usize, usize> = HashMap::new();
    for (i, sym) in symbols.iter().enumerate() {
        if !is_global(sym) {
            continue;
        }
        let name = std::str::from_utf8(symbols.symbol_name(endian, sym)?).unwrap_or("");
        if name.is_empty() {
            continue;
        }
        if !asxxxx::valid_symbol_name(name) || name == asxxxx::ABS_SYMBOL {
            return Err(Error::new(format!(
                "symbol {name:?} cannot be named in a .rel object"
            )));
        }
        let value = sym.st_value(endian);
        let def = match sym.st_shndx(endian) {
            elf::SHN_UNDEF => None,
            elf::SHN_ABS => Some(Def { area: None, value }),
            elf::SHN_COMMON => {
                return Err(Error::new(format!(
                    "'{name}' is a common symbol, which SDCC objects cannot hold; \
                     compile with -fno-common"
                )));
            }
            shndx => match placed.get(&usize::from(shndx)) {
                Some(p) => Some(Def {
                    area: Some(p.area),
                    value: p.offset.checked_add(value).ok_or_else(|| {
                        Error::new(format!("{name:?} lies beyond any address space"))
                    })?,
                }),
                None => continue,
            },
        };
        if sym.st_bind() == elf::STB_WEAK {
            warnings.push(format!("weak symbol '{name}' becomes an ordinary one"));
        }
        symbol_index.insert(i, model_symbols.len());
        model_symbols.push(Symbol {
            name: name.to_string(),
            def,
        });
    }

    for section in sections.iter() {
        let sh_type = section.sh_type(endian);
        let applies = placed.contains_key(&(section.sh_info(endian) as usize));
        if matches!(sh_type, elf::SHT_REL | elf::SHT_CREL) && applies {
            return Err(Error::new(
                "relocations without explicit addends are not supported",
            ));
        }
        if sh_type != elf::SHT_RELA {
            continue;
        }
        let Some(&Placed {
            contents,
            offset: base,
            data_size,
            ..
        }) = placed.get(&(section.sh_info(endian) as usize))
        else {
            continue;
        };
        for rela in section.data_as_array::<Rela32<LE>, _>(endian, data)? {
            let r_type = rela.r_type(endian);
            let r_offset = rela.r_offset(endian);
            let row = match from_elf(r_type) {
                FromElf::Field(row) => row,
                FromElf::Ignore => continue,
                FromElf::Unsupported => {
                    return Err(Error::new(format!(
                        "unsupported relocation type {r_type} at offset {r_offset:#06X}"
                    )));
                }
            };
            if u64::from(r_offset) + u64::from(row.field.size()) > u64::from(data_size) {
                return Err(Error::new(format!(
                    "relocation at offset {r_offset:#06X} lies outside its section's data"
                )));
            }
            let r_sym = rela.r_sym(endian) as usize;
            let addend = i64::from(rela.r_addend(endian)) + row.pc_bias;
            // The addend is written into the field, which would cut off what
            // does not fit.
            let bits = match row.field {
                Field::Word => 16,
                Field::Byte => 8,
                _ => 32,
            };
            if addend < -(1 << (bits - 1)) || addend >= 1 << bits {
                return Err(Error::new(format!(
                    "addend {addend} at offset {r_offset:#06X} does not fit its field"
                )));
            }
            // Symbol 0 stands for address zero.
            let sym = match r_sym {
                0 => None,
                _ => Some(symbols.symbol(object::SymbolIndex(r_sym))?),
            };
            let (target, addend) = if let Some(sym) = sym.filter(|s| is_global(s)) {
                let s = *symbol_index.get(&r_sym).ok_or_else(|| {
                    let name = symbols.symbol_name(endian, sym).unwrap_or_default();
                    Error::new(format!(
                        "relocation against '{}', which has no area",
                        String::from_utf8_lossy(name)
                    ))
                })?;
                (Target::Symbol(s), addend)
            } else if let Some(sym) = sym {
                let p = placed
                    .get(&usize::from(sym.st_shndx(endian)))
                    .ok_or_else(|| {
                        Error::new(format!(
                            "relocation at offset {r_offset:#06X} into a section with no area"
                        ))
                    })?;
                let value = i64::from(sym.st_value(endian)) + i64::from(p.offset);
                (Target::Area(p.area), addend + value)
            } else {
                (Target::Absolute, addend)
            };
            areas[contents].relocs.push(Reloc {
                offset: base + r_offset,
                field: row.field,
                pc_relative: row.pc_relative,
                unsigned: false,
                page: Page::None,
                target,
                // As the field reads back. An offset into an area, added
                // above, is taken modulo the field by the linker anyway.
                addend: sign_extend(addend, bits),
            });
        }
    }

    for area in &mut areas {
        area.relocs.sort_by_key(|r| r.offset);
        if let Some(w) = area
            .relocs
            .windows(2)
            .find(|w| w[1].offset < w[0].offset + w[0].field.size())
        {
            return Err(Error::new(format!(
                "relocations at {:#06X} and {:#06X} in {} overlap",
                w[0].offset, w[1].offset, area.name
            )));
        }
    }

    Ok((keep_used_areas(areas, model_symbols, module), warnings))
}

/// The area named `name`, added after the others if there is none.
fn area_index(areas: &mut Vec<Area>, name: &str) -> usize {
    match areas.iter().position(|a| a.name == name) {
        Some(i) => i,
        None => {
            areas.push(Area {
                name: name.to_string(),
                ..Area::default()
            });
            areas.len() - 1
        }
    }
}

/// Where a section landed: the area its symbols and references go to, the
/// area holding its bytes, and its offset in both.
#[derive(Clone, Copy)]
struct Placed {
    area: usize,
    contents: usize,
    offset: u32,
    /// Bytes the section holds, which relocations must lie within.
    data_size: u32,
}

/// `module`, which usually comes from a file name, with each character the
/// linker would stop at replaced by `_`.
fn module_name(module: &str) -> String {
    let keep = |c: char| asxxxx::valid_area_name(c.encode_utf8(&mut [0; 4]));
    module
        .chars()
        .map(|c| if keep(c) { c } else { '_' })
        .collect()
}

fn sign_extend(value: i64, bits: u32) -> i64 {
    let shift = 64 - bits;
    (value << shift) >> shift
}

fn is_global(sym: &elf::Sym32<LE>) -> bool {
    matches!(sym.st_bind(), elf::STB_GLOBAL | elf::STB_WEAK)
}

/// Drop the areas that are empty, define no symbol and no relocation refers
/// to.
fn keep_used_areas(areas: Vec<Area>, mut symbols: Vec<Symbol>, module: &str) -> Object {
    let defines = |i: usize| {
        symbols
            .iter()
            .any(|s| s.def.is_some_and(|d| d.area == Some(i)))
    };
    let referred = |i: usize| {
        areas
            .iter()
            .any(|a| a.relocs.iter().any(|r| r.target == Target::Area(i)))
    };
    let used: Vec<bool> = (0..areas.len())
        .map(|i| areas[i].size > 0 || defines(i) || referred(i))
        .collect();
    let mut new_index = vec![None; areas.len()];
    let mut kept = Vec::new();
    for (i, area) in areas.into_iter().enumerate() {
        if used[i] {
            new_index[i] = Some(kept.len());
            kept.push(area);
        }
    }

    for symbol in &mut symbols {
        if let Some(def) = &mut symbol.def {
            def.area = def.area.and_then(|a| new_index[a]);
        }
    }
    for area in &mut kept {
        for reloc in &mut area.relocs {
            if let Target::Area(a) = &mut reloc.target {
                *a = new_index[*a].expect("an area relocations refer to is kept");
            }
        }
    }

    Object {
        name: Some(module_name(module)),
        areas: kept,
        symbols,
        ..Object::default()
    }
}
