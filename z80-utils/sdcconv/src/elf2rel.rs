use std::collections::HashMap;

use asxxxx::{Area, Chunk, Def, Object, Reloc, Symbol, Target};
use object::LittleEndian as LE;
use object::elf::{self, FileHeader32, Rela32};
use object::read::elf::{FileHeader, Rela, SectionHeader, Sym};

use crate::Error;
use crate::areas::{AREAS, INITIALIZED, INITIALIZER, Place, area_for_section};
use crate::relocs::{EM_Z80, FromElf, from_elf};

/// Convert a Z80 ELF relocatable object to an ASxxxx object named `module`.
pub fn elf_to_rel(data: &[u8], module: &str) -> Result<Object, Error> {
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
        let (area, contents) = match area_for_section(name, sh_type, flags) {
            Place::Area(a) => {
                let a = area_index(&mut areas, a);
                (a, a)
            }
            Place::Initialized => (
                area_index(&mut areas, INITIALIZED),
                area_index(&mut areas, INITIALIZER),
            ),
        };
        let offset = areas[area].size;
        placed.insert(
            i,
            Placed {
                area,
                contents,
                offset,
            },
        );
        let size = section.sh_size(endian);
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
        let def = match sym.st_shndx(endian) {
            elf::SHN_UNDEF => None,
            shndx => match placed.get(&usize::from(shndx)) {
                Some(p) => Some(Def {
                    area: Some(p.area),
                    value: p.offset + sym.st_value(endian),
                }),
                None => continue,
            },
        };
        symbol_index.insert(i, model_symbols.len());
        model_symbols.push(Symbol {
            name: name.to_string(),
            def,
        });
    }

    for section in sections.iter() {
        if section.sh_type(endian) != elf::SHT_RELA {
            continue;
        }
        let Some(&Placed {
            contents,
            offset: base,
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
            let r_sym = rela.r_sym(endian) as usize;
            if r_sym == 0 {
                return Err(Error::new(format!(
                    "relocation without a symbol at offset {r_offset:#06X}"
                )));
            }
            let addend = i64::from(rela.r_addend(endian)) + row.pc_bias;
            let sym = symbols.symbol(object::SymbolIndex(r_sym))?;
            let (target, addend) = if is_global(sym) {
                let s = *symbol_index.get(&r_sym).ok_or_else(|| {
                    let name = symbols.symbol_name(endian, sym).unwrap_or_default();
                    Error::new(format!(
                        "relocation against '{}', which has no area",
                        String::from_utf8_lossy(name)
                    ))
                })?;
                (Target::Symbol(s), addend)
            } else {
                let p = placed
                    .get(&usize::from(sym.st_shndx(endian)))
                    .ok_or_else(|| {
                        Error::new(format!(
                            "relocation at offset {r_offset:#06X} into a section with no area"
                        ))
                    })?;
                let value = i64::from(sym.st_value(endian)) + i64::from(p.offset);
                (Target::Area(p.area), addend + value)
            };
            areas[contents].relocs.push(Reloc {
                offset: base + r_offset,
                width: row.width,
                pc_relative: row.pc_relative,
                target,
                addend,
            });
        }
    }

    keep_used_areas(areas, model_symbols, module)
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
}

fn is_global(sym: &elf::Sym32<LE>) -> bool {
    matches!(sym.st_bind(), elf::STB_GLOBAL | elf::STB_WEAK)
}

/// Drop the areas that are empty and define no symbol.
fn keep_used_areas(
    areas: Vec<Area>,
    mut symbols: Vec<Symbol>,
    module: &str,
) -> Result<Object, Error> {
    let defines = |i: usize| {
        symbols
            .iter()
            .any(|s| s.def.is_some_and(|d| d.area == Some(i)))
    };
    let used: Vec<bool> = areas
        .iter()
        .enumerate()
        .map(|(i, a)| a.size > 0 || defines(i))
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
            if let Target::Area(a) = reloc.target {
                let a = new_index[a]
                    .ok_or_else(|| Error::new(format!("relocation against empty area {a}")))?;
                reloc.target = Target::Area(a);
            }
        }
    }

    Ok(Object {
        name: Some(module.to_string()),
        areas: kept,
        symbols,
        ..Object::default()
    })
}
