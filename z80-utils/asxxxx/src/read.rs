use crate::raw::{self, Entry, Record, TextKind};
use crate::write::symbol_order;
use crate::{
    ABS_SYMBOL, Area, Chunk, Def, Error, Field, Object, Page, PageBase, PdkPart, Reloc, Symbol,
    T_LINE_VALUES, Target, address_space, mode,
};

/// How a linker reads relocation fields. The Padauk linker merges every field
/// into an instruction, so a Padauk object reads differently from the rest.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Linker {
    Standard,
    Pdk,
}

impl Linker {
    /// The linker for the port SDCC recorded in an object's `O` line.
    fn of(raw: &raw::Object) -> Linker {
        let pdk = raw.records.iter().any(|r| {
            matches!(r, Record::Options(o) if o.split_whitespace().any(|w| w.starts_with("-mpdk")))
        });
        if pdk { Linker::Pdk } else { Linker::Standard }
    }
}

/// Parse the text of a `.rel` file, reading it for the linker its `O` line
/// names. A Padauk object without one, as written from assembly source,
/// needs [`parse_as`].
pub fn parse(text: &str) -> Result<Object, Error> {
    let (raw, lines) = raw::parse_lines(text)?;
    Decoder::new(&raw, Linker::of(&raw), &lines).run()
}

/// Parse the text of a `.rel` file, reading it for `linker`.
pub fn parse_as(text: &str, linker: Linker) -> Result<Object, Error> {
    let (raw, lines) = raw::parse_lines(text)?;
    Decoder::new(&raw, linker, &lines).run()
}

struct Decoder<'a> {
    raw: &'a raw::Object,
    linker: Linker,
    /// The line each record starts on.
    lines: &'a [usize],
    obj: Object,
    /// Where each `S` line went in `obj.symbols`, in file order, which is the
    /// order relocations count in. `.__.ABS.` is not kept.
    symbol_index: Vec<Option<usize>>,
    /// The base page definition in effect.
    page: Option<usize>,
}

impl<'a> Decoder<'a> {
    fn new(raw: &'a raw::Object, linker: Linker, lines: &'a [usize]) -> Self {
        Decoder {
            raw,
            linker,
            lines,
            obj: Object {
                format: raw.format,
                ..Object::default()
            },
            symbol_index: Vec::new(),
            page: None,
        }
    }

    fn error(&self, record: usize, message: impl Into<String>) -> Error {
        Error::new(self.lines[record], message)
    }

    fn run(mut self) -> Result<Object, Error> {
        // Like the linker, take the declarations first and the contents
        // after, so a T line may name an area or symbol declared below it.
        let mut header = None;
        let mut current_area = None;
        for (i, record) in self.raw.records.iter().enumerate() {
            match record {
                Record::Header { areas, symbols } => header = Some((*areas, *symbols)),
                Record::Module(name) => self.obj.name = Some(name.clone()),
                Record::Options(text) => self.obj.options.push(text.clone()),
                Record::Area {
                    name,
                    size,
                    flags,
                    addr,
                } => {
                    self.obj.areas.push(Area {
                        name: name.clone(),
                        flags: *flags,
                        addr: addr.unwrap_or(0),
                        size: *size,
                        ..Area::default()
                    });
                    current_area = Some(self.obj.areas.len() - 1);
                }
                Record::Symbol {
                    name,
                    defined,
                    value,
                } => {
                    if name == ABS_SYMBOL {
                        // The linker takes its value like any symbol's, and
                        // a fixed address is read as relative to zero.
                        if !defined || *value != 0 || current_area.is_some() {
                            let message = format!("{ABS_SYMBOL} is not zero ahead of the areas");
                            return Err(self.error(i, message));
                        }
                        self.symbol_index.push(None);
                        continue;
                    }
                    if !defined && *value != 0 {
                        return Err(self.error(i, format!("reference to '{name}' has a value")));
                    }
                    self.symbol_index.push(Some(self.obj.symbols.len()));
                    self.obj.symbols.push(Symbol {
                        name: name.clone(),
                        def: defined.then_some(Def {
                            area: current_area,
                            value: *value,
                        }),
                    });
                }
                Record::Text { .. } => {}
            }
        }

        // List the symbols as write does, so a file that lists them in
        // another order reads the same once rewritten.
        let order = symbol_order(&self.obj)?;
        let mut moved = vec![0; order.len()];
        for (new, &old) in order.iter().enumerate() {
            moved[old] = new;
        }
        self.obj.symbols = order.iter().map(|&s| self.obj.symbols[s].clone()).collect();
        for s in self.symbol_index.iter_mut().flatten() {
            *s = moved[*s];
        }

        for (i, record) in self.raw.records.iter().enumerate() {
            if let Record::Text {
                bytes,
                kind,
                area,
                entries,
            } = record
            {
                let area = usize::from(*area);
                if area >= self.obj.areas.len() {
                    return Err(self.error(i, format!("names undefined area {area}")));
                }
                if bytes.len() > T_LINE_VALUES {
                    return Err(
                        self.error(i, format!("T line holds more than {T_LINE_VALUES} values"))
                    );
                }
                match kind {
                    TextKind::Relocate => self.text(i, bytes, area, entries)?,
                    TextKind::Page => self.page_base(i, bytes, area, entries)?,
                }
            }
        }

        // The linker sizes its tables from the header, reading the counts as
        // a C int, and only minds a file that overflows them.
        let capacity = |n: u32| i32::try_from(n).map_or(0, |n| n as usize);
        if let Some((areas, symbols)) = header
            && (capacity(areas) < self.obj.areas.len()
                || capacity(symbols) < self.symbol_index.len())
        {
            return Err(Error::new(
                0,
                format!(
                    "header allows {areas} areas and {symbols} symbols, file has {} and {}",
                    self.obj.areas.len(),
                    self.symbol_index.len()
                ),
            ));
        }
        for area in &mut self.obj.areas {
            area.relocs.sort_by_key(|r| r.offset);
        }
        Ok(self.obj)
    }

    fn text(
        &mut self,
        i: usize,
        bytes: &[u8],
        area: usize,
        entries: &[Entry],
    ) -> Result<(), Error> {
        let n = usize::from(self.obj.format.addr_bytes);
        if bytes.len() < n {
            return Err(self.error(i, "T line shorter than an address"));
        }
        let addr = self.obj.format.read_uint(&bytes[..n]) as u32;
        let data = &bytes[n..];

        // The linker places each field after the ones before it on the line.
        if entries.windows(2).any(|w| w[1].pos <= w[0].pos) {
            return Err(self.error(i, "relocation entries out of order"));
        }
        let mut contents = Vec::with_capacity(data.len());
        let mut relocs = Vec::new();
        let mut next = 0;
        for &e in entries {
            let pos = usize::from(e.pos)
                .checked_sub(n)
                .ok_or_else(|| self.error(i, "relocation inside the T line address"))?;
            let shape = self
                .field(e.mode, data.get(pos..).unwrap_or(&[]))
                .map_err(|m| self.error(i, m))?;
            if pos < next || pos + shape.len > data.len() {
                return Err(self.error(i, "relocation field overlaps another or the line end"));
            }
            contents.extend_from_slice(&data[next..pos]);
            relocs.push(Reloc {
                // From the start of the line, until the line is placed.
                offset: contents.len() as u32,
                field: shape.field,
                pc_relative: e.mode & mode::PCR != 0,
                unsigned: e.mode & mode::USGN != 0,
                page: self.page(e.mode).map_err(|m| self.error(i, m))?,
                target: if fixed_jump(e) {
                    Target::Absolute
                } else {
                    self.target(i, e)?
                },
                addend: self.obj.format.read_int(&data[pos..pos + shape.value_len]),
            });
            contents.resize(contents.len() + shape.field.size() as usize, 0);
            next = pos + shape.len;
        }
        contents.extend_from_slice(&data[next..]);

        let end = u64::from(addr) + contents.len() as u64;
        if end > address_space(self.obj.format) {
            let message = format!("T line at {addr:X} runs past the address space");
            return Err(self.error(i, message));
        }
        let area = &mut self.obj.areas[area];
        let base = if area.is_absolute() { area.addr } else { 0 };
        if !contents.is_empty() && (addr < base || end > u64::from(base) + u64::from(area.size)) {
            let message = format!("T line at {addr:X} lies outside {}", area.name);
            return Err(self.error(i, message));
        }
        for reloc in &mut relocs {
            reloc.offset += addr;
        }
        area.relocs.extend(relocs);
        insert_chunk(&mut area.data, addr, contents).map_err(|m| self.error(i, m))
    }

    fn page_base(
        &mut self,
        i: usize,
        bytes: &[u8],
        area: usize,
        entries: &[Entry],
    ) -> Result<(), Error> {
        let relocs = entries
            .iter()
            .map(|e| Ok((e.pos, e.mode, self.target(i, *e)?)))
            .collect::<Result<_, Error>>()?;
        self.obj.pages.push(PageBase {
            area,
            bytes: bytes.to_vec(),
            relocs,
        });
        self.page = Some(self.obj.pages.len() - 1);
        Ok(())
    }

    fn target(&self, i: usize, e: Entry) -> Result<Target, Error> {
        let index = usize::from(e.index);
        if e.mode & mode::SYM == 0 {
            return if index < self.obj.areas.len() {
                Ok(Target::Area(index))
            } else {
                Err(self.error(i, format!("relocation against undefined area {index}")))
            };
        }
        match self.symbol_index.get(index) {
            Some(Some(s)) => Ok(Target::Symbol(*s)),
            Some(None) => Ok(Target::Absolute),
            None => Err(self.error(i, format!("relocation against undefined symbol {index}"))),
        }
    }

    fn page(&self, bits: u16) -> Result<Page, String> {
        match (bits & mode::PAG0 != 0, bits & mode::PAG != 0) {
            (false, false) => Ok(Page::None),
            (true, false) => Ok(Page::Zero(self.page)),
            (false, true) => Ok(Page::Paged(self.page)),
            (true, true) => Err(format!("relocation mode {bits:03X} names two pages")),
        }
    }

    /// The field a mode describes, following the linker's tests in order.
    /// Combinations whose extra bits the linker ignores are refused, since
    /// nothing could write them back faithfully.
    fn field(&self, bits: u16, at: &[u8]) -> Result<Shape, String> {
        use mode::*;
        let known = BYTE | SYM | PCR | BYTX | USGN | PAG0 | PAG | MSB | BYT3 | HIB | BIT;
        let unsupported = || format!("unsupported relocation mode {bits:03X}");
        if bits & !known != 0 {
            return Err(unsupported());
        }
        let n = usize::from(self.obj.format.addr_bytes);
        let has = |b: u16| bits & b != 0;
        let shape = match self.linker {
            Linker::Pdk => {
                if has(BYT3 | HIB | BIT) {
                    return Err(unsupported());
                }
                let part = match bits & (BYTE | BYTX | MSB) {
                    BYTX => PdkPart::Whole,
                    x if x == BYTE | BYTX => PdkPart::Low,
                    x if x == BYTE | BYTX | MSB => PdkPart::High,
                    _ => return Err(unsupported()),
                };
                let [_, _, op_lo, op_hi, version, ..] = *at else {
                    return Err("Padauk field runs past the line end".into());
                };
                Shape::new(
                    Field::Pdk {
                        part,
                        opcode: u16::from_le_bytes([op_lo, op_hi]),
                        version,
                    },
                    5,
                    2,
                )
            }
            Linker::Standard if has(BYTE) => {
                match (has(BYT3), has(BYTX), has(MSB), has(HIB), has(BIT)) {
                    (true, bytx, false, false, true) => Shape::new(Field::LongBit { bytx }, 3, 3),
                    (true, bytx, msb, hib, false) if !(msb && hib) => {
                        let index = if hib { 2 } else { u8::from(msb) };
                        Shape::new(Field::LongByte { index, bytx }, 3, 3)
                    }
                    (false, true, false, false, true) => Shape::new(Field::AddrBit, n, n),
                    (false, true, msb, false, false) => {
                        Shape::new(Field::AddrByte(u8::from(msb)), n, n)
                    }
                    (false, false, false, false, false) => Shape::new(Field::Byte, 1, 1),
                    _ => return Err(unsupported()),
                }
            }
            Linker::Standard => {
                if has(BYT3 | HIB | BIT) {
                    return Err(unsupported());
                }
                match (has(BYTX), has(MSB)) {
                    (false, false) => Shape::new(Field::Word, 2, 2),
                    (false, true) => Shape::new(Field::Word24, 3, 3),
                    (true, false) => {
                        let Some(&opcode) = at.get(2) else {
                            return Err("jump field runs past the line end".into());
                        };
                        Shape::new(Field::Jump11 { opcode }, 3, 2)
                    }
                    // sdas390 writes this for a 19-bit jump, which the linker
                    // does not recognise.
                    (true, true) => return Err(unsupported()),
                }
            }
        };
        Ok(shape)
    }
}

/// Whether `e` is an 11-bit jump to a fixed address, which sdas8051 writes
/// against area FFFF and the linker reads as zero. Only `R` lines have these.
fn fixed_jump(e: Entry) -> bool {
    let jump11 = e.mode & (mode::BYTE | mode::BYTX | mode::MSB) == mode::BYTX;
    jump11 && e.mode & mode::SYM == 0 && e.index == 0xFFFF
}

/// A field as it sits in a `T` line: `len` values, the first `value_len` of
/// which hold the value to relocate.
struct Shape {
    field: Field,
    len: usize,
    value_len: usize,
}

impl Shape {
    fn new(field: Field, len: usize, value_len: usize) -> Shape {
        Shape {
            field,
            len,
            value_len,
        }
    }
}

fn insert_chunk(chunks: &mut Vec<Chunk>, offset: u32, bytes: Vec<u8>) -> Result<(), String> {
    if bytes.is_empty() {
        return Ok(());
    }
    let end = offset + bytes.len() as u32;
    let i = chunks.partition_point(|c| c.offset < offset);
    let touches_prev = match i.checked_sub(1).map(|j| &chunks[j]) {
        Some(prev) if prev.end() > offset => return Err(overlap(offset)),
        Some(prev) => prev.end() == offset,
        None => false,
    };
    let touches_next = match chunks.get(i) {
        Some(next) if next.offset < end => return Err(overlap(offset)),
        Some(next) => next.offset == end,
        None => false,
    };
    match (touches_prev, touches_next) {
        (true, true) => {
            let next = chunks.remove(i);
            let prev = &mut chunks[i - 1];
            prev.bytes.extend(bytes);
            prev.bytes.extend(next.bytes);
        }
        (true, false) => chunks[i - 1].bytes.extend(bytes),
        (false, true) => {
            let next = &mut chunks[i];
            let tail = std::mem::replace(&mut next.bytes, bytes);
            next.bytes.extend(tail);
            next.offset = offset;
        }
        (false, false) => chunks.insert(i, Chunk { offset, bytes }),
    }
    Ok(())
}

fn overlap(offset: u32) -> String {
    format!("T line at {offset:X} overlaps earlier contents")
}

#[cfg(test)]
mod tests {
    use crate::parse;

    #[test]
    fn rejects_malformed_input() {
        let head = "XL4\nH 1 areas 1 global symbols\nS .__.ABS. Def00000000\nA _CODE size 1 flags 0 addr 0\n";
        for (body, line, message) in [
            (
                "S _y Ref00000000\n",
                0,
                "header allows 1 areas and 1 symbols, file has 1 and 2",
            ),
            ("R 00 00 00 00\n", 5, "R line without a T line"),
            (
                "T 00 00 00 00 00\nR 00 00 00 00 81 04 00 00\n",
                5,
                "unsupported relocation mode 081",
            ),
            (
                "T 00 00 00 00 00 00\nR 00 00 00 00 02 04 07 00\n",
                5,
                "relocation against undefined symbol 7",
            ),
            ("S _x Ref00000001\n", 5, "reference to '_x' has a value"),
            ("S _\u{1} Ref00000000\n", 5, "control character in line"),
            ("T 00 00 00 00 00\n", 5, "T line without an R or P line"),
            (
                "T 00 00 00 00 00 00\nR 00 00 00 00\n",
                5,
                "T line at 0 lies outside _CODE",
            ),
            (
                "T 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\nR 00 00 00 00\n",
                5,
                "T line holds more than 16 values",
            ),
            (
                "T 00 00 00 00 00 00 00 00\nR 00 00 00 00 00 06 00 00 00 04 00 00\n",
                5,
                "relocation entries out of order",
            ),
            (
                "S .__.ABS. Def00000000\n",
                5,
                ".__.ABS. is not zero ahead of the areas",
            ),
            (
                "T 00 00 00 00 00 00\nP 00 00 00 00 08 02 FF FF\n",
                5,
                "relocation against undefined area 65535",
            ),
            (
                "T FF FF FF FF 00 00 00\nR 00 00 00 00\n",
                5,
                "T line at FFFFFFFF runs past the address space",
            ),
            (
                "H 1 areas 1 global symbols\n",
                5,
                "more than one module in a file is not supported",
            ),
        ] {
            let err = parse(&format!("{head}{body}")).unwrap_err();
            assert_eq!(
                (err.line, err.message.as_str()),
                (line, message),
                "{body:?}"
            );
        }
        let err = parse("XL4\nH 0 areas 1 global symbols\nS .__.ABS. Def00000020\n").unwrap_err();
        assert_eq!(
            (err.line, err.message.as_str()),
            (3, ".__.ABS. is not zero ahead of the areas")
        );
    }
}
