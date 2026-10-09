use crate::raw::{self, Entry, Record, TextKind};
use crate::{
    ABS_SYMBOL, Area, Chunk, Error, Field, Object, Page, PdkPart, Reloc, T_LINE_VALUES, Target,
    mode, valid_name,
};

/// Write `obj` as the text of a `.rel` file.
pub fn write(obj: &Object) -> Result<String, Error> {
    Ok(raw::write(&encode(obj)?))
}

/// Turn `obj` into the records of a `.rel` file. Symbols defined in an area
/// are listed under it and the rest come first, each group in model order.
/// Lines that need no base page definition come before any, and each
/// definition is followed by the lines that use it.
fn encode(obj: &Object) -> Result<raw::Object, Error> {
    let addr_bytes = obj.format.addr_bytes;
    if !(2..=4).contains(&addr_bytes) {
        let message = format!("unsupported address width {addr_bytes}");
        return Err(Error::new(0, message));
    }
    check_text(obj)?;
    let order = symbol_order(obj)?;
    let mut file_index = vec![0; obj.symbols.len()];
    for (i, &s) in order.iter().enumerate() {
        file_index[s] = (i + 1) as u16;
    }

    let mut e = Encoder {
        obj,
        file_index,
        records: Vec::new(),
        lines: vec![Vec::new(); obj.pages.len() + 1],
    };
    e.records.push(Record::Header {
        areas: obj.areas.len() as u32,
        symbols: obj.symbols.len() as u32 + 1,
    });
    if let Some(name) = &obj.name {
        e.records.push(Record::Module(name.clone()));
    }
    for option in &obj.options {
        e.records.push(Record::Options(option.clone()));
    }
    e.records.push(Record::Symbol {
        name: ABS_SYMBOL.to_string(),
        defined: true,
        value: 0,
    });
    let mut order = order.iter().peekable();
    let mut symbols_in = |records: &mut Vec<Record>, area: Option<usize>| {
        while let Some(&s) = order.next_if(|&&s| defined_in(obj, s) == area) {
            let symbol = &obj.symbols[s];
            records.push(Record::Symbol {
                name: symbol.name.clone(),
                defined: symbol.def.is_some(),
                value: symbol.def.map_or(0, |d| d.value),
            });
        }
    };
    symbols_in(&mut e.records, None);
    for (i, area) in obj.areas.iter().enumerate() {
        e.records.push(Record::Area {
            name: area.name.clone(),
            size: area.size,
            flags: area.flags,
            addr: Some(area.addr),
        });
        symbols_in(&mut e.records, Some(i));
    }

    for (i, area) in obj.areas.iter().enumerate() {
        e.contents(i, area)?;
    }
    let mut lines = std::mem::take(&mut e.lines).into_iter();
    e.records.extend(lines.next().unwrap_or_default());
    for (page, page_lines) in lines.enumerate() {
        e.page_base(page)?;
        e.records.extend(page_lines);
    }
    Ok(raw::Object {
        format: obj.format,
        records: e.records,
    })
}

/// Refuse names and lines that would not read back the same.
fn check_text(obj: &Object) -> Result<(), Error> {
    let changes = |text: &str| Error::new(0, format!("{text:?} would not read back the same"));
    for area in &obj.areas {
        if !valid_name(&area.name) {
            return Err(changes(&area.name));
        }
    }
    for symbol in &obj.symbols {
        if !valid_name(&symbol.name) || symbol.name == ABS_SYMBOL {
            return Err(changes(&symbol.name));
        }
    }
    for line in obj.name.iter().chain(&obj.options) {
        if line.trim() != line || line.contains(char::is_control) {
            return Err(changes(line));
        }
    }
    Ok(())
}

fn defined_in(obj: &Object, symbol: usize) -> Option<usize> {
    obj.symbols[symbol].def.and_then(|d| d.area)
}

/// The order symbols are written in: absolute definitions and references
/// first, then each area's definitions.
pub(crate) fn symbol_order(obj: &Object) -> Result<Vec<usize>, Error> {
    let mut groups = vec![Vec::new(); obj.areas.len() + 1];
    for (i, symbol) in obj.symbols.iter().enumerate() {
        let group = match defined_in(obj, i) {
            Some(area) if area >= obj.areas.len() => {
                return Err(Error::new(
                    0,
                    format!("'{}' is defined in undefined area {area}", symbol.name),
                ));
            }
            Some(area) => area + 1,
            None => 0,
        };
        groups[group].push(i);
    }
    Ok(groups.concat())
}

struct Encoder<'a> {
    obj: &'a Object,
    /// Each symbol's index among the `S` lines.
    file_index: Vec<u16>,
    records: Vec<Record>,
    /// Content lines by the base page definition they need, the first for
    /// those that need none.
    lines: Vec<Vec<Record>>,
}

impl Encoder<'_> {
    fn contents(&mut self, area_index: usize, area: &Area) -> Result<(), Error> {
        let mut relocs: Vec<Reloc> = area.relocs.clone();
        relocs.sort_by_key(|r| r.offset);
        let outside = |r: &Reloc| {
            Error::new(
                0,
                format!(
                    "relocation at {:X} in {} is outside its contents",
                    r.offset, area.name
                ),
            )
        };

        let addr_bytes = self.obj.format.addr_bytes;
        let room = T_LINE_VALUES - usize::from(addr_bytes);
        let mut next = 0;
        for chunk in &area.data {
            let mut pos = chunk.offset;
            while pos < chunk.end() {
                let (start, first) = (pos, next);
                let mut used = 0;
                let mut page = None;
                while pos < chunk.end() {
                    match relocs.get(next) {
                        Some(r) if r.offset < pos => return Err(outside(r)),
                        Some(r) if r.offset == pos => {
                            if r.offset + r.field.size() > chunk.end() {
                                return Err(outside(r));
                            }
                            let len = field_len(r.field, addr_bytes);
                            let needs = page_needed(r);
                            if used + len > room
                                || needs.is_some() && page.is_some() && needs != page
                            {
                                break;
                            }
                            page = page.or(needs);
                            used += len;
                            pos += r.field.size();
                            next += 1;
                            if miscounted(r.field) {
                                break;
                            }
                        }
                        r => {
                            let until = r.map_or(chunk.end(), |r| r.offset.min(chunk.end()));
                            let take = (until - pos).min((room - used) as u32);
                            if take == 0 {
                                break;
                            }
                            used += take as usize;
                            pos += take;
                        }
                    }
                }
                let page = page.flatten();
                self.line(area_index, chunk, start, pos, &relocs[first..next], page)?;
            }
        }
        match relocs.get(next) {
            Some(r) => Err(outside(r)),
            None => Ok(()),
        }
    }

    fn line(
        &mut self,
        area_index: usize,
        chunk: &Chunk,
        start: u32,
        stop: u32,
        relocs: &[Reloc],
        page: Option<usize>,
    ) -> Result<(), Error> {
        let group = page.map_or(0, |p| p + 1);
        if group >= self.lines.len() {
            return Err(Error::new(0, format!("undefined base page {}", group - 1)));
        }

        let format = self.obj.format;
        let contents = |from: u32, to: u32| {
            &chunk.bytes[(from - chunk.offset) as usize..(to - chunk.offset) as usize]
        };
        if u64::from(start) >> (8 * format.addr_bytes) != 0 {
            return Err(Error::new(
                0,
                format!(
                    "contents at {start:X} in {} lie beyond the address width",
                    self.obj.areas[area_index].name
                ),
            ));
        }
        let mut bytes = Vec::new();
        format.push_int(&mut bytes, i64::from(start), usize::from(format.addr_bytes));
        let mut entries = Vec::new();
        let mut pos = start;
        for r in relocs {
            bytes.extend_from_slice(contents(pos, r.offset));
            entries.push(Entry {
                mode: self.mode(r)?,
                pos: bytes.len() as u8,
                index: self.index(r.target)?,
            });
            self.field_bytes(&mut bytes, r);
            pos = r.offset + r.field.size();
        }
        bytes.extend_from_slice(contents(pos, stop));
        self.lines[group].push(Record::Text {
            bytes,
            kind: TextKind::Relocate,
            area: area_index as u16,
            entries,
        });
        Ok(())
    }

    fn page_base(&mut self, p: usize) -> Result<(), Error> {
        let page = &self.obj.pages[p];
        let entries = page
            .relocs
            .iter()
            .map(|&(pos, mode, target)| {
                Ok(Entry {
                    mode,
                    pos,
                    index: self.index(target)?,
                })
            })
            .collect::<Result<_, Error>>()?;
        self.records.push(Record::Text {
            bytes: page.bytes.clone(),
            kind: TextKind::Page,
            area: page.area as u16,
            entries,
        });
        Ok(())
    }

    fn index(&self, target: Target) -> Result<u16, Error> {
        match target {
            Target::Symbol(s) => {
                self.file_index.get(s).copied().ok_or_else(|| {
                    Error::new(0, format!("relocation against undefined symbol {s}"))
                })
            }
            Target::Area(a) if a < self.obj.areas.len() => Ok(a as u16),
            Target::Area(a) => Err(Error::new(
                0,
                format!("relocation against undefined area {a}"),
            )),
        }
    }

    fn mode(&self, r: &Reloc) -> Result<u16, Error> {
        use mode::*;
        let invalid = || Error::new(0, format!("invalid field {:?}", r.field));
        let bytx_if = |bytx: bool| if bytx { BYTX } else { 0 };
        let field = match r.field {
            Field::Word => 0,
            Field::Word24 => MSB,
            Field::Byte => BYTE,
            Field::AddrByte(0) => BYTE | BYTX,
            Field::AddrByte(1) => BYTE | BYTX | MSB,
            Field::AddrByte(_) => return Err(invalid()),
            Field::LongByte { index, bytx } => {
                let pick = match index {
                    0 => 0,
                    1 => MSB,
                    2 => HIB,
                    _ => return Err(invalid()),
                };
                BYTE | BYT3 | pick | bytx_if(bytx)
            }
            Field::AddrBit => BYTE | BYTX | BIT,
            Field::LongBit { bytx } => BYTE | BYT3 | BIT | bytx_if(bytx),
            Field::Jump11 { .. } => BYTX,
            Field::Pdk { part, .. } => match part {
                PdkPart::Whole => BYTX,
                PdkPart::Low => BYTE | BYTX,
                PdkPart::High => BYTE | BYTX | MSB,
            },
        };
        let page = match r.page {
            Page::None => 0,
            Page::Zero(_) => PAG0,
            Page::Paged(_) => PAG,
        };
        let sym = if matches!(r.target, Target::Symbol(_)) {
            SYM
        } else {
            0
        };
        let pcr = if r.pc_relative { PCR } else { 0 };
        let usgn = if r.unsigned { USGN } else { 0 };
        Ok(field | page | sym | pcr | usgn)
    }

    fn field_bytes(&self, bytes: &mut Vec<u8>, r: &Reloc) {
        let format = self.obj.format;
        let value_len = match r.field {
            Field::Word | Field::Jump11 { .. } | Field::Pdk { .. } => 2,
            Field::Word24 | Field::LongByte { .. } | Field::LongBit { .. } => 3,
            Field::Byte => 1,
            Field::AddrByte(_) | Field::AddrBit => usize::from(format.addr_bytes),
        };
        format.push_int(bytes, r.addend, value_len);
        match r.field {
            Field::Jump11 { opcode } => bytes.push(opcode),
            Field::Pdk {
                opcode, version, ..
            } => {
                bytes.extend(opcode.to_le_bytes());
                bytes.push(version);
            }
            _ => {}
        }
    }
}

/// The base page definition a relocation needs, `Some(None)` for the
/// default one.
fn page_needed(r: &Reloc) -> Option<Option<usize>> {
    match r.page {
        Page::None => None,
        Page::Zero(p) | Page::Paged(p) => Some(p),
    }
}

/// Values a field takes up in a `T` line.
fn field_len(field: Field, addr_bytes: u8) -> usize {
    match field {
        Field::Word => 2,
        Field::Word24 | Field::LongByte { .. } | Field::LongBit { .. } | Field::Jump11 { .. } => 3,
        Field::Byte => 1,
        Field::AddrByte(_) | Field::AddrBit => usize::from(addr_bytes),
        Field::Pdk { .. } => 5,
    }
}

/// Whether the linker locates later fields on the line wrongly after this
/// one: it hides two of a 24-bit field's bytes but only counts them when the
/// address-width flag is set.
fn miscounted(field: Field) -> bool {
    matches!(
        field,
        Field::LongByte { bytx: false, .. } | Field::LongBit { bytx: false }
    )
}

#[cfg(test)]
mod tests {
    use crate::{Area, Chunk, Def, Field, Object, Page, Reloc, Symbol, Target, write};

    #[test]
    fn layout() {
        let mut bytes = vec![0; 20];
        bytes[0] = 0x3E;
        bytes[13..20].copy_from_slice(&[0xDD, 0x86, 0x3E, 0, 0xC9, 0, 0xC9]);
        let reloc = |offset, field, target, addend| Reloc {
            offset,
            field,
            pc_relative: false,
            unsigned: false,
            page: Page::None,
            target,
            addend,
        };
        let obj = Object {
            name: Some("m".into()),
            symbols: vec![
                Symbol {
                    name: "_f".into(),
                    def: Some(Def {
                        area: Some(0),
                        value: 0,
                    }),
                },
                Symbol {
                    name: "_ext".into(),
                    def: None,
                },
            ],
            areas: vec![Area {
                name: "_CODE".into(),
                size: 20,
                data: vec![Chunk { offset: 0, bytes }],
                relocs: vec![
                    reloc(16, Field::AddrByte(1), Target::Symbol(1), 0x12C),
                    reloc(11, Field::Word, Target::Area(0), 5),
                ],
                ..Area::default()
            }],
            ..Object::default()
        };
        // The word at 11 would straddle the first line, and the address byte
        // at 16 takes four of the second line's sixteen values.
        let expected = "XL4\n\
            H 1 areas 3 global symbols\n\
            M m\n\
            S .__.ABS. Def00000000\n\
            S _ext Ref00000000\n\
            A _CODE size 14 flags 0 addr 0\n\
            S _f Def00000000\n\
            T 00 00 00 00 3E 00 00 00 00 00 00 00 00 00 00\n\
            R 00 00 00 00\n\
            T 0B 00 00 00 05 00 DD 86 3E 2C 01 00 00 C9 00 C9\n\
            R 00 00 00 00 00 04 00 00 8B 09 01 00\n";
        assert_eq!(write(&obj).unwrap(), expected);
    }

    #[test]
    fn refuses_names_that_would_not_read_back() {
        for name in ["", "a b", "a\u{0}", ".__.ABS."] {
            let obj = Object {
                symbols: vec![Symbol {
                    name: name.into(),
                    def: None,
                }],
                ..Object::default()
            };
            let message = format!("{name:?} would not read back the same");
            assert_eq!(write(&obj).unwrap_err().message, message);
        }
    }
}
