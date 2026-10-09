use std::fmt::Write as _;

use crate::{
    ABS_SYMBOL, Area, Chunk, Error, Object, R_BYTE, R_BYTX, R_MSB, R_PCR, R_SYM, Reloc, Target,
    Width,
};

/// Most values one `T` line may hold, address included. The linker drops any
/// beyond this.
const T_LINE_VALUES: usize = 16;

/// Write `obj` as the text of a `.rel` file.
///
/// Symbols defined in an area are listed under it and the rest come first,
/// each group in model order.
pub fn write(obj: &Object) -> Result<String, Error> {
    let order = symbol_order(obj)?;
    let mut file_index = vec![0; obj.symbols.len()];
    for (i, &s) in order.iter().enumerate() {
        file_index[s] = i + 1;
    }

    let digits = obj.format.addr_bytes as usize * 2;
    let endian = if obj.format.big_endian { 'H' } else { 'L' };
    let mut out = String::new();
    writeln!(out, "X{endian}{}", obj.format.addr_bytes).unwrap();
    writeln!(
        out,
        "H {:X} areas {:X} global symbols",
        obj.areas.len(),
        obj.symbols.len() + 1
    )
    .unwrap();
    if let Some(name) = &obj.name {
        writeln!(out, "M {name}").unwrap();
    }
    for option in &obj.options {
        writeln!(out, "O {option}").unwrap();
    }
    writeln!(out, "S {ABS_SYMBOL} Def{:0digits$X}", 0).unwrap();

    let mut order = order.iter().peekable();
    let mut symbols_in = |out: &mut String, area: Option<usize>| {
        while let Some(symbol) = order.next_if(|&&s| defined_in(obj, s) == area) {
            let symbol = &obj.symbols[*symbol];
            match symbol.def {
                Some(def) => writeln!(out, "S {} Def{:0digits$X}", symbol.name, def.value),
                None => writeln!(out, "S {} Ref{:0digits$X}", symbol.name, 0),
            }
            .unwrap();
        }
    };
    symbols_in(&mut out, None);
    for (i, area) in obj.areas.iter().enumerate() {
        writeln!(
            out,
            "A {} size {:X} flags {:X} addr {:X}",
            area.name, area.size, area.flags, area.addr
        )
        .unwrap();
        symbols_in(&mut out, Some(i));
    }

    for (i, area) in obj.areas.iter().enumerate() {
        write_contents(&mut out, obj, i, area, &file_index)?;
    }
    Ok(out)
}

fn defined_in(obj: &Object, symbol: usize) -> Option<usize> {
    obj.symbols[symbol].def.and_then(|d| d.area)
}

/// The order symbols are written in: absolute definitions and references
/// first, then each area's definitions.
fn symbol_order(obj: &Object) -> Result<Vec<usize>, Error> {
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

fn write_contents(
    out: &mut String,
    obj: &Object,
    area_index: usize,
    area: &Area,
    file_index: &[usize],
) -> Result<(), Error> {
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

    let room = T_LINE_VALUES - obj.format.addr_bytes as usize;
    let mut next = 0;
    for chunk in &area.data {
        let mut pos = chunk.offset;
        while pos < chunk.end() {
            let (start, first) = (pos, next);
            let mut used = 0;
            while pos < chunk.end() {
                match relocs.get(next) {
                    Some(r) if r.offset < pos => return Err(outside(r)),
                    Some(r) if r.offset == pos => {
                        if r.offset + r.width.size() > chunk.end() {
                            return Err(outside(r));
                        }
                        let field = field_len(r.width, obj.format.addr_bytes);
                        if used + field > room {
                            break;
                        }
                        used += field;
                        pos += r.width.size();
                        next += 1;
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
            write_line(
                out,
                obj,
                area_index,
                chunk,
                start,
                pos,
                &relocs[first..next],
                file_index,
            )?;
        }
    }
    match relocs.get(next) {
        Some(r) => Err(outside(r)),
        None => Ok(()),
    }
}

#[allow(clippy::too_many_arguments)]
fn write_line(
    out: &mut String,
    obj: &Object,
    area_index: usize,
    chunk: &Chunk,
    start: u32,
    stop: u32,
    relocs: &[Reloc],
    file_index: &[usize],
) -> Result<(), Error> {
    let format = obj.format;
    let n = format.addr_bytes as usize;
    let contents = |from: u32, to: u32| {
        &chunk.bytes[(from - chunk.offset) as usize..(to - chunk.offset) as usize]
    };

    let mut t = Vec::new();
    push_value(&mut t, u64::from(start), n, format.big_endian);
    let mut r = Vec::new();
    push_value(&mut r, 0, 2, format.big_endian);
    push_value(&mut r, area_index as u64, 2, format.big_endian);

    let mut pos = start;
    for reloc in relocs {
        t.extend_from_slice(contents(pos, reloc.offset));
        let mode = match reloc.width {
            Width::Word => 0,
            Width::Byte => R_BYTE,
            Width::AddrLow => R_BYTE | R_BYTX,
            Width::AddrHigh => R_BYTE | R_BYTX | R_MSB,
        };
        let pcr = if reloc.pc_relative { R_PCR } else { 0 };
        let (sym, index) = match reloc.target {
            Target::Symbol(s) => match file_index.get(s) {
                Some(&index) => (R_SYM, index),
                None => {
                    return Err(Error::new(
                        0,
                        format!("relocation against undefined symbol {s}"),
                    ));
                }
            },
            Target::Area(a) if a < obj.areas.len() => (0, a),
            Target::Area(a) => {
                return Err(Error::new(
                    0,
                    format!("relocation against undefined area {a}"),
                ));
            }
        };
        r.extend([mode | pcr | sym, t.len() as u8]);
        push_value(&mut r, index as u64, 2, format.big_endian);
        push_value(
            &mut t,
            reloc.addend as u64,
            field_len(reloc.width, format.addr_bytes),
            format.big_endian,
        );
        pos = reloc.offset + reloc.width.size();
    }
    t.extend_from_slice(contents(pos, stop));

    for (kind, bytes) in [('T', t), ('R', r)] {
        out.push(kind);
        for b in bytes {
            write!(out, " {b:02X}").unwrap();
        }
        out.push('\n');
    }
    Ok(())
}

/// Values a field takes up in a `T` line.
fn field_len(width: Width, addr_bytes: u8) -> usize {
    match width {
        Width::Word => 2,
        Width::Byte => 1,
        Width::AddrLow | Width::AddrHigh => addr_bytes as usize,
    }
}

fn push_value(buf: &mut Vec<u8>, value: u64, len: usize, big_endian: bool) {
    let bytes = (0..len).map(|i| (value >> (8 * i)) as u8);
    if big_endian {
        buf.extend(bytes.rev());
    } else {
        buf.extend(bytes);
    }
}

#[cfg(test)]
mod tests {
    use crate::{Area, Chunk, Def, Object, Reloc, Symbol, Target, Width, write};

    #[test]
    fn layout() {
        let mut bytes = vec![0; 20];
        bytes[0] = 0x3E;
        bytes[13..20].copy_from_slice(&[0xDD, 0x86, 0x3E, 0, 0xC9, 0, 0xC9]);
        let reloc = |offset, width, target, addend| Reloc {
            offset,
            width,
            pc_relative: false,
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
                    reloc(16, Width::AddrHigh, Target::Symbol(1), 0x12C),
                    reloc(11, Width::Word, Target::Area(0), 5),
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
}
