use crate::{
    ABS_SYMBOL, Area, Chunk, Def, Error, Object, R_BYTE, R_BYTX, R_MSB, R_PCR, R_SYM, Reloc,
    Symbol, Target, Width,
};

/// Parse the text of a `.rel` file.
pub fn parse(text: &str) -> Result<Object, Error> {
    let mut parser = Parser {
        line: 0,
        radix: 16,
        obj: Object::default(),
        header: None,
        symbol_index: Vec::new(),
        current_area: None,
        pending: None,
    };
    // Like the linker, take the declarations first and the contents after,
    // so a T or R line may name an area or symbol declared below it.
    let mut contents = Vec::new();
    for (i, line) in text.lines().enumerate() {
        let line = line.trim();
        if line.starts_with(['T', 'R']) {
            contents.push((i + 1, line));
        } else {
            parser.line = i + 1;
            parser.record(line)?;
        }
    }
    for (number, line) in contents {
        parser.line = number;
        parser.record(line)?;
    }
    parser.finish()
}

struct Parser {
    line: usize,
    radix: u32,
    obj: Object,
    header: Option<(usize, usize)>,
    /// Where each `S` line went in `obj.symbols`, in file order, which is the
    /// order relocations count in. `.__.ABS.` is not kept.
    symbol_index: Vec<Option<usize>>,
    current_area: Option<usize>,
    /// A `T` line waiting for the `R` line that says which area it is in.
    pending: Option<(u32, Vec<u8>)>,
}

impl Parser {
    fn error(&self, message: impl Into<String>) -> Error {
        Error::new(self.line, message)
    }

    fn record(&mut self, line: &str) -> Result<(), Error> {
        let Some(kind) = line.chars().next() else {
            return Ok(());
        };
        let rest = line[kind.len_utf8()..].trim();
        match kind {
            'X' | 'D' | 'Q' => self.format_line(kind, rest),
            'H' => self.header_line(rest),
            'M' => {
                self.obj.name = Some(rest.to_string());
                Ok(())
            }
            'O' => {
                self.obj.options.push(rest.to_string());
                Ok(())
            }
            'A' => self.area_line(rest),
            'S' => self.symbol_line(rest),
            'T' => self.t_line(rest),
            'R' => self.r_line(rest),
            _ => Err(self.error(format!("unsupported record '{kind}'"))),
        }
    }

    fn format_line(&mut self, kind: char, rest: &str) -> Result<(), Error> {
        self.radix = match kind {
            'X' => 16,
            'D' => 10,
            _ => 8,
        };
        for c in rest.chars() {
            match c {
                'L' => self.obj.format.big_endian = false,
                'H' => self.obj.format.big_endian = true,
                '2' | '3' | '4' => self.obj.format.addr_bytes = c as u8 - b'0',
                _ => return Err(self.error(format!("unknown format flag '{c}'"))),
            }
        }
        Ok(())
    }

    fn header_line(&mut self, rest: &str) -> Result<(), Error> {
        let fields: Vec<&str> = rest.split_whitespace().collect();
        let [areas, "areas", symbols, "global", "symbols"] = fields[..] else {
            return Err(self.error("malformed header"));
        };
        self.header = Some((self.number(areas)? as usize, self.number(symbols)? as usize));
        Ok(())
    }

    fn area_line(&mut self, rest: &str) -> Result<(), Error> {
        let mut fields = rest.split_whitespace();
        let name = fields
            .next()
            .ok_or_else(|| self.error("area without a name"))?;
        let mut area = Area {
            name: name.to_string(),
            ..Area::default()
        };
        let (mut size, mut flags) = (None, None);
        while let Some(key) = fields.next() {
            let value = fields
                .next()
                .ok_or_else(|| self.error(format!("area attribute '{key}' without a value")))?;
            let value = self.number(value)?;
            match key {
                "size" => size = Some(value),
                "flags" => flags = Some(value),
                "addr" => area.addr = value,
                _ => return Err(self.error(format!("unknown area attribute '{key}'"))),
            }
        }
        area.size = size.ok_or_else(|| self.error("area without a size"))?;
        area.flags = flags.ok_or_else(|| self.error("area without flags"))?;
        self.obj.areas.push(area);
        self.current_area = Some(self.obj.areas.len() - 1);
        Ok(())
    }

    fn symbol_line(&mut self, rest: &str) -> Result<(), Error> {
        let fields: Vec<&str> = rest.split_whitespace().collect();
        let [name, kind] = fields[..] else {
            return Err(self.error("malformed symbol"));
        };
        let (defined, value) = if let Some(value) = kind.strip_prefix("Def") {
            (true, self.number(value)?)
        } else if let Some(value) = kind.strip_prefix("Ref") {
            (false, self.number(value)?)
        } else {
            return Err(self.error(format!("symbol '{name}' is neither Def nor Ref")));
        };
        if name == ABS_SYMBOL {
            self.symbol_index.push(None);
            return Ok(());
        }
        if !defined && value != 0 {
            return Err(self.error(format!("reference to '{name}' has a value")));
        }
        self.symbol_index.push(Some(self.obj.symbols.len()));
        self.obj.symbols.push(Symbol {
            name: name.to_string(),
            def: defined.then_some(Def {
                area: self.current_area,
                value,
            }),
        });
        Ok(())
    }

    fn t_line(&mut self, rest: &str) -> Result<(), Error> {
        if self.pending.is_some() {
            return Err(self.error("T line follows a T line"));
        }
        let bytes = self.bytes(rest)?;
        let n = self.obj.format.addr_bytes as usize;
        if bytes.len() < n {
            return Err(self.error("T line shorter than an address"));
        }
        self.pending = Some((self.value(&bytes[..n]) as u32, bytes[n..].to_vec()));
        Ok(())
    }

    fn r_line(&mut self, rest: &str) -> Result<(), Error> {
        let (addr, data) = self
            .pending
            .take()
            .ok_or_else(|| self.error("R line without a T line"))?;
        let bytes = self.bytes(rest)?;
        if bytes.len() < 4 || bytes[..2] != [0, 0] || bytes.len() % 4 != 0 {
            return Err(self.error("malformed R line"));
        }
        let area_index = self.value(&bytes[2..4]) as usize;
        if area_index >= self.obj.areas.len() {
            return Err(self.error(format!("R line names undefined area {area_index}")));
        }

        let n = self.obj.format.addr_bytes as usize;
        let mut entries = Vec::new();
        for entry in bytes[4..].chunks(4) {
            let pos = (entry[1] as usize)
                .checked_sub(n)
                .ok_or_else(|| self.error("relocation inside the T line address"))?;
            entries.push((pos, entry[0], self.value(&entry[2..4]) as usize));
        }
        entries.sort_by_key(|&(pos, _, _)| pos);

        let mut contents = Vec::with_capacity(data.len());
        let mut relocs = Vec::new();
        let mut next = 0;
        for (pos, mode, index) in entries {
            let width = self.width(mode)?;
            let field = match width {
                Width::Word => 2,
                Width::Byte => 1,
                Width::AddrLow | Width::AddrHigh => n,
            };
            if pos < next || pos + field > data.len() {
                return Err(self.error("relocation field overlaps another or the line end"));
            }
            contents.extend_from_slice(&data[next..pos]);
            let target = if mode & R_SYM != 0 {
                Target::Symbol(self.symbol(index)?)
            } else if index < self.obj.areas.len() {
                Target::Area(index)
            } else {
                return Err(self.error(format!("relocation against undefined area {index}")));
            };
            relocs.push(Reloc {
                offset: addr + contents.len() as u32,
                width,
                pc_relative: mode & R_PCR != 0,
                target,
                addend: sign_extend(self.value(&data[pos..pos + field]), field * 8),
            });
            contents.resize(contents.len() + width.size() as usize, 0);
            next = pos + field;
        }
        contents.extend_from_slice(&data[next..]);

        let area = &mut self.obj.areas[area_index];
        area.relocs.extend(relocs);
        insert_chunk(&mut area.data, addr, contents).map_err(|m| self.error(m))
    }

    fn finish(mut self) -> Result<Object, Error> {
        if self.pending.is_some() {
            return Err(self.error("T line without an R line"));
        }
        // The linker sizes its tables from the header and only minds a file
        // that overflows them.
        if let Some((areas, symbols)) = self.header
            && (areas < self.obj.areas.len() || symbols < self.symbol_index.len())
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

    fn width(&self, mode: u8) -> Result<Width, Error> {
        let known = R_BYTE | R_SYM | R_PCR | R_BYTX | R_MSB;
        let byte = mode & R_BYTE != 0;
        let bytx = mode & R_BYTX != 0;
        let msb = mode & R_MSB != 0;
        match (mode & !known == 0, byte, bytx, msb) {
            (true, false, false, false) => Ok(Width::Word),
            (true, true, false, false) => Ok(Width::Byte),
            (true, true, true, false) => Ok(Width::AddrLow),
            (true, true, true, true) => Ok(Width::AddrHigh),
            _ => Err(self.error(format!("unsupported relocation mode {mode:02X}"))),
        }
    }

    fn symbol(&self, index: usize) -> Result<usize, Error> {
        match self.symbol_index.get(index) {
            Some(Some(i)) => Ok(*i),
            Some(None) => Err(self.error(format!("relocation against {ABS_SYMBOL}"))),
            None => Err(self.error(format!("relocation against undefined symbol {index}"))),
        }
    }

    fn number(&self, text: &str) -> Result<u32, Error> {
        u32::from_str_radix(text, self.radix)
            .map_err(|_| self.error(format!("'{text}' is not a number")))
    }

    fn bytes(&self, text: &str) -> Result<Vec<u8>, Error> {
        text.split_whitespace()
            .map(|t| {
                u8::try_from(self.number(t)?)
                    .map_err(|_| self.error(format!("'{t}' is not a byte")))
            })
            .collect()
    }

    /// The little- or big-endian value of `bytes`.
    fn value(&self, bytes: &[u8]) -> u64 {
        let fold = |v: u64, &b: &u8| (v << 8) | u64::from(b);
        if self.obj.format.big_endian {
            bytes.iter().fold(0, fold)
        } else {
            bytes.iter().rev().fold(0, fold)
        }
    }
}

fn sign_extend(value: u64, bits: usize) -> i64 {
    let shift = 64 - bits;
    ((value << shift) as i64) >> shift
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
                "T 00 00 00 00 00\nR 00 00 00 00 11 04 00 00\n",
                6,
                "unsupported relocation mode 11",
            ),
            (
                "T 00 00 00 00 00 00\nR 00 00 00 00 02 04 07 00\n",
                6,
                "relocation against undefined symbol 7",
            ),
            ("S _x Ref00000001\n", 5, "reference to '_x' has a value"),
            ("T 00 00 00 00 00\n", 5, "T line without an R line"),
        ] {
            let err = parse(&format!("{head}{body}")).unwrap_err();
            assert_eq!(
                (err.line, err.message.as_str()),
                (line, message),
                "{body:?}"
            );
        }
    }
}
