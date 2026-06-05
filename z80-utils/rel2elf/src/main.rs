//! rel2elf — Convert SDCC .rel/.lib files to Z80/SM83 ELF format
//!
//! 3-phase pipeline:
//!   Phase 1: Parse  (.rel text → RawModule)
//!   Phase 2: Transform (RawModule → ElfModule)
//!   Phase 3: Emit   (ElfModule → ELF binary)

use std::collections::HashMap;
use std::env;
use std::fs;
use std::io::Write;
use std::path::Path;
use std::process;

// ── Constants ────────────────────────────────────────────────────────────────

const EM_Z80: u16 = 0x1F90;
const ET_REL: u16 = 1;
const ELFCLASS32: u8 = 1;
const ELFDATA2LSB: u8 = 1;
const EV_CURRENT: u8 = 1;

const SHT_NULL: u32 = 0;
const SHT_PROGBITS: u32 = 1;
const SHT_SYMTAB: u32 = 2;
const SHT_STRTAB: u32 = 3;
const SHT_RELA: u32 = 4;
const SHT_NOBITS: u32 = 8;

const SHF_ALLOC: u32 = 0x2;
const SHF_WRITE: u32 = 0x1;
const SHF_EXECINSTR: u32 = 0x4;

const STB_LOCAL: u8 = 0;
const STB_GLOBAL: u8 = 1;
const STT_NOTYPE: u8 = 0;
const STT_SECTION: u8 = 3;
const SHN_UNDEF: u16 = 0;
const SHN_ABS: u16 = 0xFFF1;

const R_Z80_ADDR8: u32 = 2;
const R_Z80_ADDR16: u32 = 3;
const R_Z80_ADDR16_LO: u32 = 4;
const R_Z80_ADDR16_HI: u32 = 5;
const R_Z80_PCREL_8: u32 = 6;
const R_Z80_PCREL_16: u32 = 12;
const R_SM83_LDH8: u32 = 19;

const R_BYTE: u8 = 0x01;
const R_SYM: u8 = 0x02;
const R_PCR: u8 = 0x04;
const R_BYTX: u8 = 0x08;
const R_MSB: u8 = 0x80;

const AREA_FLAG_ABS: u32 = 0x08;
const AR_MAGIC: &[u8; 8] = b"!<arch>\n";

// Symbols from SDCC libraries that conflict with Rust's compiler_builtins.
// Renamed with __sdcc suffix so GBDK internally links its own versions
// while Rust runtime links the compiler_builtins versions.
// See z80-utils/rel2elf/README.md for details.
const CONFLICTING_SYMBOLS: &[&str] = &[
    "_memcpy", "_memmove", "_memset",
    "_strlen", "_strcmp",
];

fn mangle_symbol(name: &str) -> String {
    for &conflict in CONFLICTING_SYMBOLS {
        if name == conflict {
            return format!("{}__sdcc", name);
        }
    }
    name.to_string()
}

// ── Types ────────────────────────────────────────────────────────────────────

// Phase 1 output
struct RawReloc {
    mode: u8,
    t_offset: u8,
    index: u16,
}

struct RawTRPair {
    area_idx: usize,
    t_addr: u32,
    t_data: Vec<u8>,
    relocs: Vec<RawReloc>,
}

struct RawArea {
    name: String,
    size: u32,
    flags: u32,
    addr: u32,
}

struct RawSymbol {
    name: String,
    is_defined: bool,
    value: u32,
    area_index: Option<usize>,
}

struct RawModule {
    addr_bytes: usize,
    areas: Vec<RawArea>,
    symbols: Vec<RawSymbol>,
    tr_pairs: Vec<RawTRPair>,
}

// Phase 2 intermediate
struct AreaReloc {
    offset: u32,
    mode: u8,
    index: u16,
    bytx_value: Option<u32>,
}

struct AreaBuffer {
    data: Vec<u8>,
    relocs: Vec<AreaReloc>,
    strip_map: Vec<(u32, u32)>, // sorted (original_offset, cumulative_removal)
}

// Phase 2 output
struct ElfReloc {
    offset: u32,
    r_type: u32,
    sym_idx: u32,
    addend: i32,
}

struct ElfSection {
    name: String,
    sh_type: u32,
    sh_flags: u32,
    sh_addr: u32,
    sh_size: u32,
    data: Vec<u8>,
    relocs: Vec<ElfReloc>,
}

struct ElfSymbol {
    name: String,
    value: u32,
    section_idx: u16,
    binding: u8,
    sym_type: u8,
}

struct ElfModule {
    sections: Vec<ElfSection>,
    symbols: Vec<ElfSymbol>,
}

// ── AR Archive Helpers ───────────────────────────────────────────────────────

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

    let mut ext_names = Vec::new();
    let mut name_offsets: Vec<Option<usize>> = Vec::new();
    for member in members {
        let short = format!("{}/", member.name);
        if short.len() <= 16 {
            name_offsets.push(None);
        } else {
            let offset = ext_names.len();
            ext_names.extend_from_slice(member.name.as_bytes());
            ext_names.extend_from_slice(b"/\n");
            name_offsets.push(Some(offset));
        }
    }

    if !ext_names.is_empty() {
        write!(buf, "{:<16}", "//").unwrap();
        write!(buf, "{:<12}", "0").unwrap();
        write!(buf, "{:<6}", "0").unwrap();
        write!(buf, "{:<6}", "0").unwrap();
        write!(buf, "{:<8}", "0").unwrap();
        write!(buf, "{:<10}", ext_names.len()).unwrap();
        buf.extend_from_slice(b"`\n");
        buf.extend_from_slice(&ext_names);
        if ext_names.len() % 2 != 0 {
            buf.push(b'\n');
        }
    }

    for (i, member) in members.iter().enumerate() {
        let name_field = match name_offsets[i] {
            Some(offset) => format!("/{}", offset),
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

// ── Phase 1: Parse ──────────────────────────────────────────────────────────

fn parse_hex_bytes(s: &str) -> Vec<u8> {
    s.split_whitespace()
        .filter_map(|h| u8::from_str_radix(h, 16).ok())
        .collect()
}

fn parse_rel(text: &str) -> Result<RawModule, String> {
    let mut addr_bytes = 4usize;
    let mut areas: Vec<RawArea> = Vec::new();
    let mut symbols: Vec<RawSymbol> = Vec::new();
    let mut tr_pairs: Vec<RawTRPair> = Vec::new();
    let mut current_area: Option<usize> = None;
    let mut pending_t: Option<(u32, Vec<u8>)> = None;

    for line in text.lines() {
        let line = line.trim();
        if line.is_empty() {
            continue;
        }

        match line.as_bytes().first() {
            Some(b'X') => {
                if line.len() >= 3 {
                    addr_bytes = (line.as_bytes()[2] - b'0') as usize;
                    addr_bytes = addr_bytes.clamp(2, 4);
                }
            }
            Some(b'H') | Some(b'O') | Some(b'M') => {}
            Some(b'S') => {
                if line.len() < 4 {
                    continue;
                }
                let rest = &line[2..];
                let (name, is_defined, value) = if let Some(pos) = rest.rfind(" Def") {
                    let val = u32::from_str_radix(&rest[pos + 4..], 16).unwrap_or(0);
                    (&rest[..pos], true, val)
                } else if let Some(pos) = rest.rfind(" Ref") {
                    let val = u32::from_str_radix(&rest[pos + 4..], 16).unwrap_or(0);
                    (&rest[..pos], false, val)
                } else {
                    continue;
                };
                symbols.push(RawSymbol {
                    name: name.to_string(),
                    is_defined,
                    value,
                    area_index: if is_defined { current_area } else { None },
                });
            }
            Some(b'A') => {
                let parts: Vec<&str> = line[2..].split_whitespace().collect();
                if parts.len() < 6 {
                    continue;
                }
                let name = parts[0].to_string();
                let size = u32::from_str_radix(parts[2], 16).unwrap_or(0);
                let flags = u32::from_str_radix(parts[4], 16).unwrap_or(0);
                let addr = if parts.len() >= 7 {
                    u32::from_str_radix(parts[6], 16).unwrap_or(0)
                } else {
                    0
                };
                areas.push(RawArea { name, size, flags, addr });
                current_area = Some(areas.len() - 1);
            }
            Some(b'T') => {
                let bytes = parse_hex_bytes(&line[2..]);
                if bytes.len() < addr_bytes {
                    continue;
                }
                let mut addr: u32 = 0;
                for i in 0..addr_bytes {
                    addr |= (bytes[i] as u32) << (i * 8);
                }
                pending_t = Some((addr, bytes[addr_bytes..].to_vec()));
            }
            Some(b'R') => {
                let bytes = parse_hex_bytes(&line[2..]);
                if bytes.len() < 4 {
                    continue;
                }
                let area_idx = bytes[2] as usize | ((bytes[3] as usize) << 8);

                let mut relocs = Vec::new();
                let mut ri = 4;
                while ri + 4 <= bytes.len() {
                    relocs.push(RawReloc {
                        mode: bytes[ri],
                        t_offset: bytes[ri + 1],
                        index: bytes[ri + 2] as u16 | ((bytes[ri + 3] as u16) << 8),
                    });
                    ri += 4;
                }

                if let Some((t_addr, t_data)) = pending_t.take() {
                    tr_pairs.push(RawTRPair {
                        area_idx,
                        t_addr,
                        t_data,
                        relocs,
                    });
                }
            }
            _ => {}
        }
    }

    Ok(RawModule {
        addr_bytes,
        areas,
        symbols,
        tr_pairs,
    })
}

// ── Phase 2: Transform ──────────────────────────────────────────────────────

// Step 2a: Build area data buffers, stripping BYTX padding inline
fn build_area_buffers(raw: &RawModule) -> Vec<AreaBuffer> {
    let addr_bytes = raw.addr_bytes;
    let pad = if addr_bytes > 1 { addr_bytes - 1 } else { 0 };

    let mut buffers: Vec<AreaBuffer> = raw
        .areas
        .iter()
        .map(|a| AreaBuffer {
            data: vec![0u8; a.size as usize],
            relocs: Vec::new(),
            strip_map: Vec::new(),
        })
        .collect();

    for tr in &raw.tr_pairs {
        let buf = &mut buffers[tr.area_idx];

        // Identify BYTX positions within T data
        let mut bytx_t_positions: Vec<usize> = Vec::new();
        for r in &tr.relocs {
            if r.mode & R_BYTE != 0 && r.mode & R_BYTX != 0 {
                bytx_t_positions.push((r.t_offset as usize) - addr_bytes);
            }
        }
        bytx_t_positions.sort();

        // Build stripped T data, tracking per-T-line cumulative adjustments
        let mut bytx_adj: Vec<(usize, usize)> = Vec::new(); // (orig_pos, cumulative)
        let stripped_data = if !bytx_t_positions.is_empty() && pad > 0 {
            let mut sd = Vec::new();
            let mut src = 0usize;
            let mut cumulative = 0usize;
            for &bpos in &bytx_t_positions {
                let keep_end = bpos + 1;
                if keep_end <= tr.t_data.len() {
                    sd.extend_from_slice(&tr.t_data[src..keep_end]);
                }
                src = std::cmp::min(keep_end + pad, tr.t_data.len());
                cumulative += pad;
                bytx_adj.push((bpos, cumulative));
                // Record area-level strip position (cumulative filled later)
                buf.strip_map.push((tr.t_addr + bpos as u32, 0));
            }
            if src < tr.t_data.len() {
                sd.extend_from_slice(&tr.t_data[src..]);
            }
            sd
        } else {
            tr.t_data.clone()
        };

        // Commit stripped data to area buffer
        let start = tr.t_addr as usize;
        let needed = start + stripped_data.len();
        if needed > buf.data.len() {
            buf.data.resize(needed, 0);
        }
        buf.data[start..start + stripped_data.len()].copy_from_slice(&stripped_data);

        // Add relocations with adjusted offsets and BYTX full values
        for r in &tr.relocs {
            let orig_data_pos = (r.t_offset as usize) - addr_bytes;
            let mut adj = 0usize;
            for &(bpos, cum) in &bytx_adj {
                if orig_data_pos > bpos {
                    adj = cum;
                }
            }
            let adjusted_offset = tr.t_addr + (orig_data_pos as u32) - (adj as u32);

            // For BYTX: extract full field value from original T data before stripping
            let bytx_value = if r.mode & R_BYTE != 0 && r.mode & R_BYTX != 0 {
                let mut val: u32 = 0;
                for i in 0..addr_bytes {
                    if orig_data_pos + i < tr.t_data.len() {
                        val |= (tr.t_data[orig_data_pos + i] as u32) << (i * 8);
                    }
                }
                Some(val)
            } else {
                None
            };

            buf.relocs.push(AreaReloc {
                offset: adjusted_offset,
                mode: r.mode,
                index: r.index,
                bytx_value,
            });
        }
    }

    // Compute area-level cumulative strip maps
    let pad32 = pad as u32;
    for buf in &mut buffers {
        buf.strip_map.sort_by_key(|&(off, _)| off);
        let mut cum = 0u32;
        for entry in &mut buf.strip_map {
            cum += pad32;
            entry.1 = cum;
        }
    }

    buffers
}

// Step 2c: Normalize absolute areas — trim padding, set VMA, adjust symbols
fn normalize_absolute_areas(
    areas: &[RawArea],
    buffers: &mut [AreaBuffer],
    symbols: &mut [RawSymbol],
) {
    for (ai, area) in areas.iter().enumerate() {
        if area.flags & AREA_FLAG_ABS == 0 || area.addr == 0 {
            continue;
        }
        let base = area.addr as usize;
        let buf = &mut buffers[ai];

        // Trim data: keep [addr .. addr+declared_size]
        let end = std::cmp::min(base + area.size as usize, buf.data.len());
        buf.data = if base <= buf.data.len() {
            buf.data[base..end].to_vec()
        } else {
            Vec::new()
        };

        // Adjust relocation offsets
        for r in &mut buf.relocs {
            r.offset -= area.addr;
        }

        // Adjust symbol values
        for sym in symbols.iter_mut() {
            if sym.is_defined && sym.area_index == Some(ai) {
                sym.value -= area.addr;
            }
        }
    }
}

// Helpers for section naming and flags
fn section_name(area_name: &str, _is_bss: bool) -> &str {
    match area_name {
        "_CODE" => ".text",
        "_DATA" => ".data",
        "_HOME" => ".text.home",
        "_GSINIT" => ".text.gsinit",
        "_GSFINAL" => ".text.gsfinal",
        "_INITIALIZED" => ".data.initialized",
        "_INITIALIZER" => ".rodata",
        "_DABS" => ".data.abs",
        "_CABS" => ".text.abs",
        other => other,
    }
}

fn section_flags(area_name: &str) -> u32 {
    match area_name {
        "_CODE" | "_HOME" | "_GSINIT" | "_GSFINAL" | "_CABS" => SHF_ALLOC | SHF_EXECINSTR,
        "_DATA" | "_INITIALIZED" | "_DABS" => SHF_ALLOC | SHF_WRITE,
        "_INITIALIZER" => SHF_ALLOC,
        _ => SHF_ALLOC,
    }
}

fn read_addend_signed(data: &[u8], offset: usize, is_byte: bool) -> i32 {
    if is_byte {
        if offset < data.len() {
            data[offset] as i8 as i32
        } else {
            0
        }
    } else if offset + 1 < data.len() {
        u16::from_le_bytes([data[offset], data[offset + 1]]) as i16 as i32
    } else {
        0
    }
}

fn read_addend_unsigned(data: &[u8], offset: usize, is_byte: bool) -> u32 {
    if is_byte {
        if offset < data.len() {
            data[offset] as u32
        } else {
            0
        }
    } else if offset + 1 < data.len() {
        u16::from_le_bytes([data[offset], data[offset + 1]]) as u32
    } else {
        0
    }
}

fn is_byte_wide(r_type: u32) -> bool {
    matches!(
        r_type,
        R_Z80_ADDR8 | R_Z80_ADDR16_LO | R_Z80_ADDR16_HI | R_Z80_PCREL_8 | R_SM83_LDH8
    )
}

// Full Phase 2 pipeline
fn transform(raw: &mut RawModule) -> ElfModule {
    // Step 2a: Build area buffers with BYTX stripping
    let mut buffers = build_area_buffers(raw);

    // Note: SDCC uses post-strip coordinates for symbol values and T addresses,
    // so NO strip_map adjustment is needed for symbols or addends.

    // Step 2c: Normalize absolute areas
    normalize_absolute_areas(&raw.areas, &mut buffers, &mut raw.symbols);

    // Build sections (one per non-empty area)
    let mut sections: Vec<ElfSection> = Vec::new();
    let mut area_to_sec: HashMap<usize, usize> = HashMap::new();

    for (ai, area) in raw.areas.iter().enumerate() {
        let buf = &buffers[ai];
        if buf.data.is_empty() && buf.relocs.is_empty() && area.size == 0 {
            continue;
        }

        // Step 2e: BSS detection
        let is_bss =
            area.name == "_DATA" && buf.relocs.is_empty() && buf.data.iter().all(|&b| b == 0);
        let sh_addr = if area.flags & AREA_FLAG_ABS != 0 {
            area.addr
        } else {
            0
        };

        let data_len = buf.data.len() as u32;
        area_to_sec.insert(ai, sections.len());
        sections.push(ElfSection {
            name: section_name(&area.name, is_bss).to_string(),
            sh_type: if is_bss { SHT_NOBITS } else { SHT_PROGBITS },
            sh_flags: section_flags(&area.name),
            sh_addr,
            sh_size: data_len,
            data: if is_bss { Vec::new() } else { buf.data.clone() },
            relocs: Vec::new(),
        });
    }

    // Build symbol table: [NULL, section syms..., global syms...]
    let mut symbols: Vec<ElfSymbol> = vec![ElfSymbol {
        name: String::new(),
        value: 0,
        section_idx: SHN_UNDEF,
        binding: STB_LOCAL,
        sym_type: STT_NOTYPE,
    }];

    // Section symbols (in area index order for determinism)
    let mut area_to_sym: HashMap<usize, u32> = HashMap::new();
    for ai in 0..raw.areas.len() {
        if let Some(&sec_idx) = area_to_sec.get(&ai) {
            area_to_sym.insert(ai, symbols.len() as u32);
            symbols.push(ElfSymbol {
                name: String::new(),
                value: 0,
                section_idx: (sec_idx + 1) as u16, // +1 for NULL section
                binding: STB_LOCAL,
                sym_type: STT_SECTION,
            });
        }
    }

    // Global symbols from S lines
    let mut sline_to_sym: HashMap<usize, u32> = HashMap::new();
    for (si, sym) in raw.symbols.iter().enumerate() {
        if sym.name == ".__.ABS." {
            continue;
        }
        sline_to_sym.insert(si, symbols.len() as u32);

        let shndx = if sym.is_defined {
            sym.area_index
                .and_then(|ai| area_to_sec.get(&ai))
                .map(|&s| (s + 1) as u16)
                .unwrap_or(SHN_ABS)
        } else {
            SHN_UNDEF
        };

        symbols.push(ElfSymbol {
            name: mangle_symbol(&sym.name),
            value: if sym.is_defined { sym.value } else { 0 },
            section_idx: shndx,
            binding: STB_GLOBAL,
            sym_type: STT_NOTYPE,
        });
    }

    // Step 2d: Build ELF relocations and zero out relocation positions
    for ai in 0..buffers.len() {
        let sec_idx = match area_to_sec.get(&ai) {
            Some(&s) => s,
            None => continue,
        };
        if buffers[ai].relocs.is_empty() {
            continue;
        }

        let mut elf_relocs = Vec::new();

        // Borrow relocs separately to avoid borrow conflicts
        let reloc_count = buffers[ai].relocs.len();
        for ri in 0..reloc_count {
            let r_mode = buffers[ai].relocs[ri].mode;
            let r_index = buffers[ai].relocs[ri].index;
            let r_offset = buffers[ai].relocs[ri].offset;
            let r_bytx = buffers[ai].relocs[ri].bytx_value;

            let is_msb = r_mode & R_MSB != 0;
            let is_bytx = r_mode & R_BYTE != 0 && r_mode & R_BYTX != 0;
            let is_byte = r_mode & R_BYTE != 0;
            let is_sym = r_mode & R_SYM != 0;
            let is_pcr = r_mode & R_PCR != 0;

            // Check HRAM reference
            let is_hram = if is_sym {
                let sym = &raw.symbols[r_index as usize];
                sym.is_defined
                    && sym
                        .area_index
                        .map_or(false, |ai| raw.areas[ai].name == "_HRAM")
            } else {
                raw.areas
                    .get(r_index as usize)
                    .map_or(false, |a| a.name == "_HRAM")
            };

            // Determine ELF relocation type
            let r_type = if is_bytx {
                if is_msb {
                    R_Z80_ADDR16_HI
                } else {
                    R_Z80_ADDR16_LO
                }
            } else {
                match (is_byte, is_pcr) {
                    (true, true) => R_Z80_PCREL_8,
                    (true, false) if is_msb => R_Z80_ADDR16_HI,
                    (true, false) => {
                        if is_hram {
                            R_SM83_LDH8
                        } else {
                            R_Z80_ADDR8
                        }
                    }
                    (false, true) => R_Z80_PCREL_16,
                    (false, false) => R_Z80_ADDR16,
                }
            };

            // Extract addend and determine ELF symbol index
            let off = r_offset as usize;
            let (elf_sym, addend) = if is_sym {
                // Symbol reference
                let sym_idx = sline_to_sym.get(&(r_index as usize)).copied().unwrap_or(0);
                let addend = if let Some(full) = r_bytx {
                    full as i32
                } else {
                    read_addend_signed(&sections[sec_idx].data, off, is_byte)
                };
                (sym_idx, addend)
            } else {
                // Area reference — use section symbol
                let ref_ai = r_index as usize;
                let sym_idx = area_to_sym.get(&ref_ai).copied().unwrap_or(0);
                let ref_area = &raw.areas[ref_ai];
                let ref_base = if ref_area.flags & AREA_FLAG_ABS != 0 {
                    ref_area.addr
                } else {
                    0
                };

                // SDCC addends are already in post-strip coordinates
                let addend = if let Some(full) = r_bytx {
                    // BYTX: use full field value, subtract base for absolute areas
                    full as i32 - ref_base as i32
                } else if ref_base > 0 && is_byte {
                    // Byte addend to absolute area: reconstruct full offset
                    let raw_byte = read_addend_unsigned(&sections[sec_idx].data, off, true);
                    let full = (ref_base & 0xFF00) | raw_byte;
                    full as i32 - ref_base as i32
                } else if ref_base > 0 {
                    // Word addend to absolute area
                    let raw_word = read_addend_unsigned(&sections[sec_idx].data, off, false);
                    raw_word as i32 - ref_base as i32
                } else {
                    // Regular area reference — addend as-is
                    read_addend_signed(&sections[sec_idx].data, off, is_byte)
                };

                (sym_idx, addend)
            };

            elf_relocs.push(ElfReloc {
                offset: r_offset,
                r_type,
                sym_idx: elf_sym,
                addend,
            });
        }

        // Zero out relocation positions in section data
        for reloc in &elf_relocs {
            let off = reloc.offset as usize;
            if is_byte_wide(reloc.r_type) {
                if off < sections[sec_idx].data.len() {
                    sections[sec_idx].data[off] = 0;
                }
            } else if off + 1 < sections[sec_idx].data.len() {
                sections[sec_idx].data[off] = 0;
                sections[sec_idx].data[off + 1] = 0;
            }
        }

        sections[sec_idx].relocs = elf_relocs;
    }

    ElfModule { sections, symbols }
}

// ── Phase 3: Emit ───────────────────────────────────────────────────────────

fn push_u8(buf: &mut Vec<u8>, v: u8) {
    buf.push(v);
}
fn push_u16(buf: &mut Vec<u8>, v: u16) {
    buf.extend_from_slice(&v.to_le_bytes());
}
fn push_u32(buf: &mut Vec<u8>, v: u32) {
    buf.extend_from_slice(&v.to_le_bytes());
}
fn push_i32(buf: &mut Vec<u8>, v: i32) {
    buf.extend_from_slice(&v.to_le_bytes());
}

fn add_string(strtab: &mut Vec<u8>, s: &str) -> u32 {
    if s.is_empty() {
        return 0;
    }
    let offset = strtab.len() as u32;
    strtab.extend_from_slice(s.as_bytes());
    strtab.push(0);
    offset
}

fn emit_elf(module: &ElfModule) -> Vec<u8> {
    let mut shstrtab = vec![0u8];
    let mut strtab = vec![0u8];

    let n_content = module.sections.len();
    let rela_indices: Vec<usize> = module
        .sections
        .iter()
        .enumerate()
        .filter(|(_, s)| !s.relocs.is_empty())
        .map(|(i, _)| i)
        .collect();
    let n_rela = rela_indices.len();

    // Section layout: [NULL, content..., rela..., symtab, strtab, shstrtab]
    let symtab_sec_idx = 1 + n_content + n_rela;
    let strtab_sec_idx = symtab_sec_idx + 1;
    let shstrtab_sec_idx = strtab_sec_idx + 1;
    let total_sections = shstrtab_sec_idx + 1;

    // Map content section index → rela section index
    let mut content_to_rela_sec: HashMap<usize, usize> = HashMap::new();
    for (ri, &ci) in rela_indices.iter().enumerate() {
        content_to_rela_sec.insert(ci, 1 + n_content + ri);
    }

    // Build symtab data
    let first_global = module
        .symbols
        .iter()
        .position(|s| s.binding == STB_GLOBAL)
        .unwrap_or(module.symbols.len()) as u32;

    let mut symtab_data = Vec::new();
    for sym in &module.symbols {
        let name_off = if sym.name.is_empty() {
            0
        } else {
            add_string(&mut strtab, &sym.name)
        };
        push_u32(&mut symtab_data, name_off);
        push_u32(&mut symtab_data, sym.value);
        push_u32(&mut symtab_data, 0); // st_size
        symtab_data.push((sym.binding << 4) | sym.sym_type);
        symtab_data.push(0); // st_other
        push_u16(&mut symtab_data, sym.section_idx);
    }

    // Build rela data blobs
    let mut rela_blobs: Vec<Vec<u8>> = Vec::new();
    for &ci in &rela_indices {
        let mut bytes = Vec::new();
        for r in &module.sections[ci].relocs {
            push_u32(&mut bytes, r.offset);
            push_u32(&mut bytes, (r.sym_idx << 8) | r.r_type);
            push_i32(&mut bytes, r.addend);
        }
        rela_blobs.push(bytes);
    }

    // Build section headers and data blobs
    struct SH {
        name_off: u32,
        sh_type: u32,
        sh_flags: u32,
        sh_addr: u32,
        sh_offset: u32,
        sh_size: u32,
        sh_link: u32,
        sh_info: u32,
        sh_addralign: u32,
        sh_entsize: u32,
    }

    let mut headers: Vec<SH> = Vec::new();
    let mut blobs: Vec<Vec<u8>> = Vec::new();

    // Section 0: NULL
    headers.push(SH {
        name_off: 0,
        sh_type: SHT_NULL,
        sh_flags: 0,
        sh_addr: 0,
        sh_offset: 0,
        sh_size: 0,
        sh_link: 0,
        sh_info: 0,
        sh_addralign: 0,
        sh_entsize: 0,
    });
    blobs.push(Vec::new());

    // Content sections
    for sec in &module.sections {
        let name_off = add_string(&mut shstrtab, &sec.name);
        headers.push(SH {
            name_off,
            sh_type: sec.sh_type,
            sh_flags: sec.sh_flags,
            sh_addr: sec.sh_addr,
            sh_offset: 0,
            sh_size: sec.sh_size,
            sh_link: 0,
            sh_info: 0,
            sh_addralign: 1,
            sh_entsize: 0,
        });
        blobs.push(sec.data.clone());
    }

    // Rela sections
    for (ri, &ci) in rela_indices.iter().enumerate() {
        let rela_name = format!(".rela{}", module.sections[ci].name);
        let name_off = add_string(&mut shstrtab, &rela_name);
        let rela_data = &rela_blobs[ri];
        headers.push(SH {
            name_off,
            sh_type: SHT_RELA,
            sh_flags: 0,
            sh_addr: 0,
            sh_offset: 0,
            sh_size: rela_data.len() as u32,
            sh_link: symtab_sec_idx as u32,
            sh_info: (ci + 1) as u32, // content section index (+1 for NULL)
            sh_addralign: 4,
            sh_entsize: 12,
        });
        blobs.push(rela_data.clone());
    }

    // .symtab
    let symtab_name_off = add_string(&mut shstrtab, ".symtab");
    headers.push(SH {
        name_off: symtab_name_off,
        sh_type: SHT_SYMTAB,
        sh_flags: 0,
        sh_addr: 0,
        sh_offset: 0,
        sh_size: symtab_data.len() as u32,
        sh_link: strtab_sec_idx as u32,
        sh_info: first_global,
        sh_addralign: 4,
        sh_entsize: 16,
    });
    blobs.push(symtab_data);

    // .strtab
    let strtab_name_off = add_string(&mut shstrtab, ".strtab");
    headers.push(SH {
        name_off: strtab_name_off,
        sh_type: SHT_STRTAB,
        sh_flags: 0,
        sh_addr: 0,
        sh_offset: 0,
        sh_size: strtab.len() as u32,
        sh_link: 0,
        sh_info: 0,
        sh_addralign: 1,
        sh_entsize: 0,
    });
    blobs.push(strtab);

    // .shstrtab
    let shstrtab_name_off = add_string(&mut shstrtab, ".shstrtab");
    headers.push(SH {
        name_off: shstrtab_name_off,
        sh_type: SHT_STRTAB,
        sh_flags: 0,
        sh_addr: 0,
        sh_offset: 0,
        sh_size: shstrtab.len() as u32,
        sh_link: 0,
        sh_info: 0,
        sh_addralign: 1,
        sh_entsize: 0,
    });
    blobs.push(shstrtab);

    assert_eq!(headers.len(), total_sections);

    // Calculate section data offsets
    let elf_header_size = 52u32;
    let mut offset = elf_header_size;
    for (i, h) in headers.iter_mut().enumerate() {
        if h.sh_type == SHT_NULL {
            h.sh_offset = 0;
            continue;
        }
        if h.sh_type == SHT_NOBITS {
            h.sh_offset = offset;
            continue;
        }
        let align = std::cmp::max(h.sh_addralign, 1);
        offset = (offset + align - 1) & !(align - 1);
        h.sh_offset = offset;
        offset += blobs[i].len() as u32;
    }

    // Align for section header table
    offset = (offset + 3) & !3;
    let sh_offset = offset;

    // Write ELF binary
    let mut buf = Vec::new();

    // ELF header (52 bytes)
    buf.extend_from_slice(b"\x7fELF");
    push_u8(&mut buf, ELFCLASS32);
    push_u8(&mut buf, ELFDATA2LSB);
    push_u8(&mut buf, EV_CURRENT);
    push_u8(&mut buf, 0); // EI_OSABI
    buf.extend_from_slice(&[0u8; 8]);
    push_u16(&mut buf, ET_REL);
    push_u16(&mut buf, EM_Z80);
    push_u32(&mut buf, 1); // e_version
    push_u32(&mut buf, 0); // e_entry
    push_u32(&mut buf, 0); // e_phoff
    push_u32(&mut buf, sh_offset);
    push_u32(&mut buf, 0); // e_flags
    push_u16(&mut buf, elf_header_size as u16);
    push_u16(&mut buf, 0); // e_phentsize
    push_u16(&mut buf, 0); // e_phnum
    push_u16(&mut buf, 40); // e_shentsize
    push_u16(&mut buf, total_sections as u16);
    push_u16(&mut buf, shstrtab_sec_idx as u16);
    assert_eq!(buf.len(), 52);

    // Write section data
    for (i, h) in headers.iter().enumerate() {
        if h.sh_type == SHT_NULL || h.sh_type == SHT_NOBITS {
            continue;
        }
        while buf.len() < h.sh_offset as usize {
            buf.push(0);
        }
        buf.extend_from_slice(&blobs[i]);
    }

    // Pad to section header table
    while buf.len() < sh_offset as usize {
        buf.push(0);
    }

    // Write section header table
    for h in &headers {
        push_u32(&mut buf, h.name_off);
        push_u32(&mut buf, h.sh_type);
        push_u32(&mut buf, h.sh_flags);
        push_u32(&mut buf, h.sh_addr);
        push_u32(&mut buf, h.sh_offset);
        push_u32(&mut buf, h.sh_size);
        push_u32(&mut buf, h.sh_link);
        push_u32(&mut buf, h.sh_info);
        push_u32(&mut buf, h.sh_addralign);
        push_u32(&mut buf, h.sh_entsize);
    }

    buf
}

// ── Conversion entry points ─────────────────────────────────────────────────

fn convert_rel_to_elf(data: &[u8]) -> Result<Vec<u8>, String> {
    let text = std::str::from_utf8(data).map_err(|_| "invalid UTF-8 in .rel file")?;
    let mut raw = parse_rel(text)?;
    let module = transform(&mut raw);
    Ok(emit_elf(&module))
}

fn convert_lib_to_ar(data: &[u8]) -> Result<Vec<u8>, String> {
    let members = parse_ar(data)?;
    let mut out_members = Vec::new();

    for member in &members {
        let o_name = Path::new(&member.name)
            .with_extension("o")
            .file_name()
            .unwrap()
            .to_string_lossy()
            .into_owned();

        match convert_rel_to_elf(&member.data) {
            Ok(elf_data) => {
                out_members.push(ArMember {
                    name: o_name,
                    data: elf_data,
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
        eprintln!("Usage: rel2elf <input.rel|input.lib> [output.o|output.a]");
        process::exit(1);
    }

    let input_path = &args[1];
    let data = fs::read(input_path).unwrap_or_else(|e| {
        eprintln!("error: cannot read '{}': {}", input_path, e);
        process::exit(1);
    });

    let is_ar = data.len() >= 8 && &data[..8] == AR_MAGIC;

    let default_ext = if is_ar { "a" } else { "o" };
    let output_path = if args.len() > 2 {
        args[2].clone()
    } else {
        Path::new(input_path)
            .with_extension(default_ext)
            .to_string_lossy()
            .into_owned()
    };

    let result = if is_ar {
        convert_lib_to_ar(&data)
    } else {
        convert_rel_to_elf(&data)
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
