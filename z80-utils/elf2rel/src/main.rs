//! elf2rel - Convert Z80 ELF object files to SDCC .rel (ASxxxx) format
//!
//! Supports:
//!   .o (ELF) → .rel (SDCC object)
//!   .a (ar archive of ELF .o) → .lib (ar archive of .rel)
//!
//! .rel format (XL4):
//!   H <areas> areas <globals> global symbols
//!   M <module_name>
//!   A <area_name> size <hex> flags <hex> addr <hex>
//!   S <symbol_name> Def|Ref <hex_value>
//!   T <4-byte addr> <data bytes...>
//!   R <4-byte area_ref> [<4-byte reloc entries>...]
//!
//! R entry (4 bytes): mode, offset, index_lo, index_hi
//!   mode bit 0: R_BYTE (byte-sized reloc, vs word/16-bit)
//!   mode bit 1: R_SYM  (symbol reference, vs area reference)
//!   mode bit 2: R_PCR  (PC-relative)

use object::elf;
use object::read::elf::{ElfFile32, FileHeader, SectionHeader};
use object::read::elf::{Rela, Sym};
use object::LittleEndian as LE;
use std::collections::HashMap;
use std::env;
use std::fs;
use std::io::Write;
use std::path::Path;
use std::process;

// Z80 ELF relocation types
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

// ASxxxx relocation mode bits
const R_BYTE: u8 = 0x01;
const R_SYM: u8 = 0x02;
const R_PCR: u8 = 0x04;

const SDCC_AREAS: &[(&str, u8)] = &[
    ("_CODE", 0),
    ("_DATA", 0),
    ("_INITIALIZED", 0),
    ("_DABS", 8),
    ("_HOME", 0),
    ("_GSINIT", 0),
    ("_GSFINAL", 0),
    ("_INITIALIZER", 0),
    ("_CABS", 8),
];

const AR_MAGIC: &[u8; 8] = b"!<arch>\n";
const ELF_MAGIC: &[u8; 4] = b"\x7fELF";

fn section_to_area(name: &str) -> &'static str {
    if name == ".text" || name.starts_with(".text.") {
        "_CODE"
    } else if name == ".data" || name.starts_with(".data.") {
        "_DATA"
    } else if name == ".bss" || name.starts_with(".bss.") {
        "_DATA"
    } else if name == ".rodata" || name.starts_with(".rodata.") {
        "_CODE"
    } else {
        "_CODE"
    }
}

// ── Ar archive helpers ──────────────────────────────────────────────────────

struct ArMember {
    name: String,
    data: Vec<u8>,
}

fn parse_ar(data: &[u8]) -> Result<Vec<ArMember>, String> {
    if data.len() < 8 || &data[..8] != AR_MAGIC {
        return Err("not an ar archive".into());
    }
    let mut pos = 8;
    let mut extended_names: Vec<u8> = Vec::new();
    let mut members = Vec::new();

    while pos + 60 <= data.len() {
        let header = &data[pos..pos + 60];
        if &header[58..60] != b"`\n" {
            return Err(format!("invalid ar header at offset {}", pos));
        }
        let name_raw = std::str::from_utf8(&header[0..16])
            .map_err(|_| "invalid ar member name")?
            .trim_end();
        let size_str = std::str::from_utf8(&header[48..58])
            .map_err(|_| "invalid ar member size")?
            .trim();
        let size: usize = size_str
            .parse()
            .map_err(|_| format!("invalid ar size: '{}'", size_str))?;
        pos += 60;

        if pos + size > data.len() {
            return Err("ar member exceeds file size".into());
        }
        let member_data = &data[pos..pos + size];
        pos += size;
        if pos % 2 != 0 {
            pos += 1;
        }

        if name_raw == "/" || name_raw == "/SYM64/" {
            continue;
        }
        if name_raw == "//" {
            extended_names = member_data.to_vec();
            continue;
        }

        let name = if let Some(stripped) = name_raw.strip_prefix('/') {
            let offset: usize = stripped
                .trim_end_matches('/')
                .trim()
                .parse()
                .map_err(|_| "invalid extended name offset")?;
            let end = extended_names[offset..]
                .iter()
                .position(|&b| b == b'/' || b == b'\n')
                .unwrap_or(extended_names.len() - offset);
            std::str::from_utf8(&extended_names[offset..offset + end])
                .map_err(|_| "invalid extended name")?
                .to_string()
        } else {
            name_raw.trim_end_matches('/').to_string()
        };

        members.push(ArMember {
            name,
            data: member_data.to_vec(),
        });
    }

    Ok(members)
}

fn write_ar(members: &[ArMember]) -> Vec<u8> {
    let mut buf = Vec::new();
    buf.extend_from_slice(AR_MAGIC);

    // Build symbol index: collect (symbol_name, member_index) pairs.
    // For .rel files, defined symbols are lines matching "S <name> Def<hex>".
    let mut sym_entries: Vec<(String, usize)> = Vec::new();
    for (i, member) in members.iter().enumerate() {
        if let Ok(text) = std::str::from_utf8(&member.data) {
            for line in text.lines() {
                if line.starts_with("S ") {
                    if let Some(rest) = line.strip_prefix("S ") {
                        if rest.contains(" Def") && !rest.starts_with(".__.ABS.") {
                            if let Some(name) = rest.split_whitespace().next() {
                                sym_entries.push((name.to_string(), i));
                            }
                        }
                    }
                }
            }
        }
    }

    // Compute member offsets (we need to know the symtab size first).
    // Symbol table body: 4-byte BE count + count*4 offsets + null-terminated strings
    let sym_count = sym_entries.len();
    let strings_size: usize = sym_entries.iter().map(|(s, _)| s.len() + 1).sum();
    let symtab_body_size = 4 + sym_count * 4 + strings_size;

    // Symbol table member header (if we have symbols)
    let symtab_total = if sym_count > 0 {
        // header (60 bytes) + body + optional padding
        let padded = symtab_body_size + (symtab_body_size % 2);
        60 + padded
    } else {
        0
    };

    // Build the GNU extended-name table for members whose "<name>/" does not
    // fit in the 16-byte name field (name longer than 15 bytes). Each long name
    // is stored as "<name>/\n" in a "//" member; the member's own name field
    // then becomes "/<byte-offset-into-the-table>".
    let mut ext_table: Vec<u8> = Vec::new();
    let mut ext_offset: Vec<Option<usize>> = Vec::with_capacity(members.len());
    for member in members {
        if member.name.len() + 1 > 16 {
            ext_offset.push(Some(ext_table.len()));
            ext_table.extend_from_slice(member.name.as_bytes());
            ext_table.extend_from_slice(b"/\n");
        } else {
            ext_offset.push(None);
        }
    }
    let ext_total = if ext_table.is_empty() {
        0
    } else {
        60 + ext_table.len() + (ext_table.len() % 2)
    };

    // Compute offset of each data member from start of file
    let mut member_offsets = Vec::with_capacity(members.len());
    let mut offset = 8 + symtab_total + ext_total; // 8 = AR_MAGIC
    for member in members {
        member_offsets.push(offset);
        offset += 60 + member.data.len();
        if member.data.len() % 2 != 0 {
            offset += 1; // padding
        }
    }

    // Write symbol table member
    if sym_count > 0 {
        write!(buf, "{:<16}", "/").unwrap();
        write!(buf, "{:<12}", "0").unwrap();
        write!(buf, "{:<6}", "0").unwrap();
        write!(buf, "{:<6}", "0").unwrap();
        write!(buf, "{:<8}", "0").unwrap();
        write!(buf, "{:<10}", symtab_body_size).unwrap();
        buf.extend_from_slice(b"`\n");

        // 4-byte big-endian count
        buf.extend_from_slice(&(sym_count as u32).to_be_bytes());
        // 4-byte big-endian offsets for each symbol's member
        for (_, member_idx) in &sym_entries {
            buf.extend_from_slice(&(member_offsets[*member_idx] as u32).to_be_bytes());
        }
        // Null-terminated symbol name strings
        for (name, _) in &sym_entries {
            buf.extend_from_slice(name.as_bytes());
            buf.push(0);
        }
        if symtab_body_size % 2 != 0 {
            buf.push(b'\n');
        }
    }

    // Write the "//" extended-name member (holds the long member names).
    if !ext_table.is_empty() {
        write!(buf, "{:<16}", "//").unwrap();
        write!(buf, "{:<12}", "0").unwrap();
        write!(buf, "{:<6}", "0").unwrap();
        write!(buf, "{:<6}", "0").unwrap();
        write!(buf, "{:<8}", "0").unwrap();
        write!(buf, "{:<10}", ext_table.len()).unwrap();
        buf.extend_from_slice(b"`\n");
        buf.extend_from_slice(&ext_table);
        if ext_table.len() % 2 != 0 {
            buf.push(b'\n');
        }
    }

    // Write data members
    for (idx, member) in members.iter().enumerate() {
        let name_field = match ext_offset[idx] {
            Some(off) => format!("/{}", off),
            None => format!("{}/", member.name),
        };
        write!(buf, "{:<16}", name_field).unwrap();
        write!(buf, "{:<12}", "0").unwrap();
        write!(buf, "{:<6}", "0").unwrap();
        write!(buf, "{:<6}", "0").unwrap();
        write!(buf, "{:<8}", "100644").unwrap();
        write!(buf, "{:<10}", member.data.len()).unwrap();
        buf.extend_from_slice(b"`\n");
        buf.extend_from_slice(&member.data);
        if member.data.len() % 2 != 0 {
            buf.push(b'\n');
        }
    }

    buf
}

// ── ELF → .rel conversion ──────────────────────────────────────────────────

struct RelReloc {
    mode: u8,
    index: u16,
}

struct AreaData {
    bytes: Vec<u8>,
    relocs: Vec<(u32, RelReloc)>,
}

fn convert_elf_to_rel(data: &[u8], module_name: &str) -> Result<Vec<u8>, String> {
    let elf = ElfFile32::<LE>::parse(data).map_err(|e| format!("cannot parse ELF: {}", e))?;
    let header = elf.elf_header();
    let endian = LE;

    let machine = header.e_machine.get(endian);
    if machine != 0x1F90 {
        return Err(format!(
            "not a Z80 ELF (e_machine=0x{:04X}, expected 0x1F90)",
            machine
        ));
    }

    let sections = header
        .sections(endian, data)
        .map_err(|e| format!("sections: {}", e))?;
    let symbols_table = sections
        .symbols(endian, data, elf::SHT_SYMTAB)
        .map_err(|e| format!("symtab: {}", e))?;

    let mut area_sizes: HashMap<&str, u32> = HashMap::new();
    let mut area_data: HashMap<&str, AreaData> = HashMap::new();

    for area in SDCC_AREAS {
        area_sizes.insert(area.0, 0);
        area_data.insert(
            area.0,
            AreaData {
                bytes: Vec::new(),
                relocs: Vec::new(),
            },
        );
    }

    let mut section_area_map: HashMap<usize, (&str, u32)> = HashMap::new();

    for (si, section) in sections.iter().enumerate() {
        let sh_type = section.sh_type(endian);
        if sh_type != elf::SHT_PROGBITS && sh_type != elf::SHT_NOBITS {
            continue;
        }
        let sh_flags = section.sh_flags(endian);
        if sh_flags & u32::from(elf::SHF_ALLOC) == 0 {
            continue;
        }
        let name = match sections.section_name(endian, section) {
            Ok(n) => {
                let n = std::str::from_utf8(n).unwrap_or("");
                if n.is_empty() {
                    continue;
                }
                n
            }
            Err(_) => continue,
        };
        if name.starts_with(".note") || name.starts_with(".comment") {
            continue;
        }

        let area_name = section_to_area(name);
        let area = area_data.get_mut(area_name).unwrap();
        let offset = area.bytes.len() as u32;
        section_area_map.insert(si, (area_name, offset));

        if sh_type == elf::SHT_PROGBITS {
            let section_data = section.data(endian, data).unwrap_or(&[]);
            area.bytes.extend_from_slice(section_data);
        } else {
            let size = section.sh_size(endian) as usize;
            area.bytes.resize(area.bytes.len() + size, 0);
        }
        *area_sizes.get_mut(area_name).unwrap() = area.bytes.len() as u32;
    }

    struct RelSymbol {
        name: String,
        is_defined: bool,
        value: u32,
        area_index: Option<usize>,
    }

    let mut rel_symbols: Vec<RelSymbol> = vec![RelSymbol {
        name: ".__.ABS.".to_string(),
        is_defined: true,
        value: 0,
        area_index: None,
    }];

    let mut elf_sym_to_rel: HashMap<u32, usize> = HashMap::new();

    for (sym_idx, sym) in symbols_table.iter().enumerate() {
        let bind = sym.st_bind();
        if bind != elf::STB_GLOBAL && bind != elf::STB_WEAK {
            continue;
        }
        let name = match symbols_table.symbol_name(endian, sym) {
            Ok(n) => {
                let n = std::str::from_utf8(n).unwrap_or("");
                if n.is_empty() {
                    continue;
                }
                n
            }
            Err(_) => continue,
        };

        let is_defined = sym.st_shndx(endian) != elf::SHN_UNDEF;
        let mut value = 0u32;
        let mut area_index = None;

        if is_defined {
            let sec_idx = sym.st_shndx(endian) as usize;
            if let Some(&(area_name, area_offset)) = section_area_map.get(&sec_idx) {
                for (ai, &(aname, _)) in SDCC_AREAS.iter().enumerate() {
                    if aname == area_name {
                        area_index = Some(ai);
                        break;
                    }
                }
                value = area_offset + sym.st_value(endian);
            }
        }

        let rel_idx = rel_symbols.len();
        rel_symbols.push(RelSymbol {
            name: name.to_string(),
            is_defined,
            value,
            area_index,
        });
        elf_sym_to_rel.insert(sym_idx as u32, rel_idx);
    }

    // Process relocations
    for (_si, section) in sections.iter().enumerate() {
        let sh_type = section.sh_type(endian);
        if sh_type != elf::SHT_RELA {
            continue;
        }

        let target_si = section.sh_info(endian) as usize;
        let (area_name, area_base_offset) = match section_area_map.get(&target_si) {
            Some(v) => *v,
            None => continue,
        };

        let area_idx = SDCC_AREAS
            .iter()
            .position(|&(n, _)| n == area_name)
            .unwrap();

        let rela_data = section.data(endian, data).unwrap_or(&[]);
        let rela_count = rela_data.len() / std::mem::size_of::<elf::Rela32<LE>>();
        let relas = object::slice_from_bytes::<elf::Rela32<LE>>(rela_data, rela_count)
            .unwrap()
            .0;

        let area = area_data.get_mut(area_name).unwrap();

        for rela in relas {
            let r_offset = rela.r_offset(endian);
            let r_type = rela.r_type(endian);
            let r_sym = rela.r_sym(endian);
            let r_addend = rela.r_addend(endian);

            if r_type == R_Z80_NONE {
                continue;
            }

            let (size, mut mode) = match r_type {
                R_Z80_ADDR16 | R_Z80_IMM16 => (2, 0u8),
                R_Z80_ADDR8 | R_Z80_IMM8 | R_SM83_LDH8 => (1, R_BYTE),
                R_Z80_PCREL_8 => (1, R_BYTE | R_PCR),
                R_Z80_PCREL_16 => (2, R_PCR),
                R_Z80_ADDR16_LO => (1, R_BYTE),
                R_Z80_ADDR16_HI => (1, R_BYTE),
                R_Z80_FK_DATA_4 | R_Z80_FK_DATA_8 => continue,
                _ => {
                    eprintln!(
                        "warning: unsupported relocation type {} at offset 0x{:04X}",
                        r_type, r_offset
                    );
                    continue;
                }
            };

            let (reloc_index, is_sym_ref) = if r_sym != 0 {
                let sym = symbols_table
                    .symbol(object::SymbolIndex(r_sym as usize))
                    .unwrap();
                let bind = sym.st_bind();

                if bind == elf::STB_GLOBAL || bind == elf::STB_WEAK {
                    match elf_sym_to_rel.get(&r_sym) {
                        Some(&idx) => (idx as u16, true),
                        None => {
                            eprintln!("warning: unmapped symbol index {}", r_sym);
                            continue;
                        }
                    }
                } else {
                    let sec_idx = sym.st_shndx(endian) as usize;
                    if let Some(&(sym_area_name, _)) = section_area_map.get(&sec_idx) {
                        let sym_area_idx = SDCC_AREAS
                            .iter()
                            .position(|&(n, _)| n == sym_area_name)
                            .unwrap();
                        (sym_area_idx as u16, false)
                    } else {
                        continue;
                    }
                }
            } else {
                (area_idx as u16, false)
            };

            if is_sym_ref {
                mode |= R_SYM;
            }

            let byte_offset = (area_base_offset + r_offset) as usize;

            let addend = if !is_sym_ref {
                let sym = symbols_table
                    .symbol(object::SymbolIndex(r_sym as usize))
                    .unwrap();
                let sym_sec = sym.st_shndx(endian) as usize;
                let sym_area_offset = section_area_map
                    .get(&sym_sec)
                    .map(|&(_, off)| off)
                    .unwrap_or(0);
                (r_addend as i64) + (sym.st_value(endian) as i64) + (sym_area_offset as i64)
            } else {
                r_addend as i64
            };

            if byte_offset < area.bytes.len() {
                match size {
                    1 => {
                        area.bytes[byte_offset] = (addend & 0xFF) as u8;
                    }
                    2 => {
                        if byte_offset + 1 < area.bytes.len() {
                            let val = addend as u16;
                            area.bytes[byte_offset] = (val & 0xFF) as u8;
                            area.bytes[byte_offset + 1] = ((val >> 8) & 0xFF) as u8;
                        }
                    }
                    _ => {}
                }
            }

            area.relocs.push((
                r_offset + area_base_offset,
                RelReloc {
                    mode,
                    index: reloc_index,
                },
            ));
        }
    }

    // Build output symbol order
    let mut output_order: Vec<usize> = Vec::new();
    output_order.push(0);

    for (i, sym) in rel_symbols.iter().enumerate().skip(1) {
        if !sym.is_defined {
            output_order.push(i);
        }
    }
    for (ai, _) in SDCC_AREAS.iter().enumerate() {
        for (i, sym) in rel_symbols.iter().enumerate().skip(1) {
            if sym.is_defined && sym.area_index == Some(ai) {
                output_order.push(i);
            }
        }
    }

    let mut rel_to_output: HashMap<usize, u16> = HashMap::new();
    for (out_idx, &rel_idx) in output_order.iter().enumerate() {
        rel_to_output.insert(rel_idx, out_idx as u16);
    }

    for (_area_name, area) in area_data.iter_mut() {
        for (_, reloc) in area.relocs.iter_mut() {
            if reloc.mode & R_SYM != 0 {
                if let Some(&new_idx) = rel_to_output.get(&(reloc.index as usize)) {
                    reloc.index = new_idx;
                }
            }
        }
    }

    // NOTE: area-reference reloc indices (R_SYM not set) will be remapped
    // after active_areas is computed, using area_index_remap.

    // Determine which areas are non-empty (have data or defined symbols)
    let active_areas: Vec<usize> = SDCC_AREAS
        .iter()
        .enumerate()
        .filter(|&(ai, &(area_name, _))| {
            let size = area_sizes.get(area_name).copied().unwrap_or(0);
            let has_syms = rel_symbols
                .iter()
                .skip(1)
                .any(|s| s.is_defined && s.area_index == Some(ai));
            size > 0 || has_syms
        })
        .map(|(ai, _)| ai)
        .collect();

    // Build old→new area index mapping
    let mut area_index_remap: HashMap<usize, usize> = HashMap::new();
    for (new_idx, &old_idx) in active_areas.iter().enumerate() {
        area_index_remap.insert(old_idx, new_idx);
    }

    // Remap symbol area indices
    for sym in rel_symbols.iter_mut() {
        if let Some(old_ai) = sym.area_index {
            sym.area_index = area_index_remap.get(&old_ai).copied();
        }
    }

    // Remap area-reference reloc indices (non-R_SYM relocations)
    for (_area_name, area) in area_data.iter_mut() {
        for (_, reloc) in area.relocs.iter_mut() {
            if reloc.mode & R_SYM == 0 {
                if let Some(&new_idx) = area_index_remap.get(&(reloc.index as usize)) {
                    reloc.index = new_idx as u16;
                }
            }
        }
    }

    // Generate .rel output
    let mut out = Vec::new();
    let global_count = rel_symbols.len();

    writeln!(out, "XL4").unwrap();
    writeln!(
        out,
        "H {} areas {} global symbols",
        active_areas.len(),
        global_count
    )
    .unwrap();
    writeln!(out, "M {}", module_name).unwrap();

    writeln!(
        out,
        "S {} Def{:08X}",
        rel_symbols[0].name, rel_symbols[0].value
    )
    .unwrap();
    for sym in &rel_symbols[1..] {
        if !sym.is_defined {
            writeln!(out, "S {} Ref{:08X}", sym.name, sym.value).unwrap();
        }
    }

    for (new_ai, &old_ai) in active_areas.iter().enumerate() {
        let (area_name, flags) = SDCC_AREAS[old_ai];
        let size = area_sizes.get(area_name).copied().unwrap_or(0);
        writeln!(
            out,
            "A {} size {:X} flags {:X} addr 0",
            area_name, size, flags
        )
        .unwrap();

        for sym in &rel_symbols[1..] {
            if sym.is_defined && sym.area_index == Some(new_ai) {
                writeln!(out, "S {} Def{:08X}", sym.name, sym.value).unwrap();
            }
        }

        let area = match area_data.get(area_name) {
            Some(a) if !a.bytes.is_empty() => a,
            _ => continue,
        };

        const T_LINE_MAX: usize = 12;
        let mut offset = 0usize;

        while offset < area.bytes.len() {
            let mut chunk_end = (offset + T_LINE_MAX).min(area.bytes.len());

            // Ensure no multi-byte relocation spans the T-line boundary.
            // If a 2-byte relocation starts at the last byte of the chunk,
            // shorten the chunk so the relocation starts in the next T-line.
            for &(reloc_offset, ref reloc) in &area.relocs {
                let reloc_off = reloc_offset as usize;
                if reloc_off < offset || reloc_off >= chunk_end {
                    continue;
                }
                let reloc_size: usize = if reloc.mode & R_BYTE != 0 { 1 } else { 2 };
                if reloc_off + reloc_size > chunk_end {
                    // This relocation would span the boundary — end chunk before it
                    chunk_end = reloc_off;
                    break;
                }
            }

            // If chunk_end didn't advance (reloc at offset itself), force at least 1 byte
            if chunk_end <= offset {
                chunk_end = offset + 1;
            }

            let chunk = &area.bytes[offset..chunk_end];

            write!(
                out,
                "T {:02X} {:02X} 00 00",
                offset & 0xFF,
                (offset >> 8) & 0xFF
            )
            .unwrap();
            for &b in chunk {
                write!(out, " {:02X}", b).unwrap();
            }
            writeln!(out).unwrap();

            write!(out, "R 00 00 {:02X} 00", new_ai).unwrap();
            for &(reloc_offset, ref reloc) in &area.relocs {
                let reloc_off = reloc_offset as usize;
                if reloc_off >= offset && reloc_off < chunk_end {
                    let t_offset = (reloc_off - offset + 4) as u8;
                    write!(
                        out,
                        " {:02X} {:02X} {:02X} {:02X}",
                        reloc.mode,
                        t_offset,
                        (reloc.index & 0xFF) as u8,
                        ((reloc.index >> 8) & 0xFF) as u8,
                    )
                    .unwrap();
                }
            }
            writeln!(out).unwrap();

            offset = chunk_end;
        }
    }

    Ok(out)
}

// ── .a → .lib conversion ───────────────────────────────────────────────────

fn convert_ar_to_lib(data: &[u8]) -> Result<Vec<u8>, String> {
    let members = parse_ar(data)?;
    let mut out_members = Vec::new();

    for member in &members {
        let module_name = Path::new(&member.name)
            .file_stem()
            .unwrap_or_default()
            .to_string_lossy()
            .into_owned();
        let rel_name = Path::new(&member.name)
            .with_extension("rel")
            .file_name()
            .unwrap()
            .to_string_lossy()
            .into_owned();

        match convert_elf_to_rel(&member.data, &module_name) {
            Ok(rel_data) => {
                out_members.push(ArMember {
                    name: rel_name,
                    data: rel_data,
                });
            }
            Err(e) => {
                eprintln!("warning: skipping '{}': {}", member.name, e);
            }
        }
    }

    Ok(write_ar(&out_members))
}

// ── Main ────────────────────────────────────────────────────────────────────

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("Usage: elf2rel <input.o|input.a> [output.rel|output.lib]");
        process::exit(1);
    }

    let input_path = &args[1];
    let data = fs::read(input_path).unwrap_or_else(|e| {
        eprintln!("error: cannot read '{}': {}", input_path, e);
        process::exit(1);
    });

    let is_ar = data.len() >= 8 && &data[..8] == AR_MAGIC;
    let is_elf = data.len() >= 4 && &data[..4] == ELF_MAGIC;

    if !is_ar && !is_elf {
        eprintln!(
            "error: '{}' is neither an ELF file nor an ar archive",
            input_path
        );
        process::exit(1);
    }

    let default_ext = if is_ar { "lib" } else { "rel" };
    let output_path = if args.len() > 2 {
        args[2].clone()
    } else {
        Path::new(input_path)
            .with_extension(default_ext)
            .to_string_lossy()
            .into_owned()
    };

    let result = if is_ar {
        convert_ar_to_lib(&data)
    } else {
        let module_name = Path::new(input_path)
            .file_stem()
            .unwrap_or_default()
            .to_string_lossy()
            .into_owned();
        convert_elf_to_rel(&data, &module_name)
    };

    match result {
        Ok(output) => {
            fs::write(&output_path, &output).unwrap_or_else(|e| {
                eprintln!("error: cannot write '{}': {}", output_path, e);
                process::exit(1);
            });
            eprintln!("{} -> {}", input_path, output_path);
        }
        Err(e) => {
            eprintln!("error: {}", e);
            process::exit(1);
        }
    }
}
