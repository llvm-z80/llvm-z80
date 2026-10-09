//! The records of a `.rel` file as written, for any ASxxxx version 3 object.

use std::fmt::Write as _;

use byteorder::{BigEndian, ByteOrder, LittleEndian};

use crate::{Error, valid_area_name};

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Object {
    pub format: Format,
    pub records: Vec<Record>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Format {
    pub radix: Radix,
    pub big_endian: bool,
    /// Width of an address: of a `T` line's address, a symbol value, and an
    /// address-byte field before the linker narrows it.
    pub addr_bytes: u8,
}

impl Default for Format {
    /// What sdasz80 and sdasgb write.
    fn default() -> Self {
        Format {
            radix: Radix::Hex,
            big_endian: false,
            addr_bytes: 4,
        }
    }
}

impl Format {
    pub(crate) fn read_uint(self, bytes: &[u8]) -> u64 {
        if self.big_endian {
            BigEndian::read_uint(bytes, bytes.len())
        } else {
            LittleEndian::read_uint(bytes, bytes.len())
        }
    }

    pub(crate) fn read_int(self, bytes: &[u8]) -> i64 {
        if self.big_endian {
            BigEndian::read_int(bytes, bytes.len())
        } else {
            LittleEndian::read_int(bytes, bytes.len())
        }
    }

    /// Append the low `len` bytes of `value`.
    pub(crate) fn push_int(self, buf: &mut Vec<u8>, value: i64, len: usize) {
        let at = buf.len();
        buf.resize(at + len, 0);
        if self.big_endian {
            BigEndian::write_int(&mut buf[at..], value, len);
        } else {
            LittleEndian::write_int(&mut buf[at..], value, len);
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Radix {
    Hex,
    Octal,
    Decimal,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Record {
    Header {
        areas: u32,
        symbols: u32,
    },
    Module(String),
    /// An `O` line, which SDCC uses to record the options a module was built
    /// with.
    Options(String),
    Area {
        name: String,
        size: u32,
        flags: u32,
        /// Written by the SDCC assemblers only.
        addr: Option<u32>,
    },
    Symbol {
        name: String,
        defined: bool,
        value: u32,
    },
    /// A `T` line and the `R` or `P` line that follows it.
    Text {
        bytes: Vec<u8>,
        kind: TextKind,
        area: u16,
        entries: Vec<Entry>,
    },
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TextKind {
    /// `R`: the bytes belong to `area` and the entries relocate them.
    Relocate,
    /// `P`: the bytes define the base page, and the entries relocate it.
    Page,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Entry {
    /// The mode bits, with an escaped mode's extra bits above the low byte.
    /// A `P` line holds only the low byte.
    pub mode: u16,
    /// Where the field starts in the `T` line, counting its address.
    pub pos: u8,
    /// A symbol, counted over the `S` lines, or an area.
    pub index: u16,
}

/// Modes at or above this, and those whose top nibble is set, are written as
/// two bytes.
const ESCAPE: u16 = 0xF0;

/// Parse the text of a `.rel` file.
pub fn parse(text: &str) -> Result<Object, Error> {
    parse_lines(text).map(|(obj, _)| obj)
}

/// Parse `text`, also returning the line each record starts on.
pub(crate) fn parse_lines(text: &str) -> Result<(Object, Vec<usize>), Error> {
    let mut obj = Object::default();
    let mut lines = Vec::new();
    let mut radix = 16;
    let mut pending: Option<(usize, Vec<u8>)> = None;
    let (mut format_seen, mut header_seen) = (false, false);
    for (i, line) in text.lines().enumerate() {
        let number = i + 1;
        let err = |message: String| Error::new(number, message);
        let line = line.trim();
        if line.contains(char::is_control) {
            return Err(err("control character in line".into()));
        }
        let Some(kind) = line.chars().next() else {
            continue;
        };
        let rest = line[kind.len_utf8()..].trim();
        let num = |t: &str| {
            u32::from_str_radix(t, radix).map_err(|_| err(format!("'{t}' is not a number")))
        };
        if pending.is_some() && !matches!(kind, 'R' | 'P') {
            return Err(err("T line without an R or P line".into()));
        }
        // The linker wants the format line first and the header next, and
        // takes a second of either as the start of another module.
        let another = || err("more than one module in a file is not supported".into());
        match kind {
            'X' | 'Q' | 'D' if format_seen => return Err(another()),
            'X' | 'Q' | 'D' => format_seen = true,
            _ if !format_seen => return Err(err("missing the format line".into())),
            'H' if header_seen => return Err(another()),
            'H' => header_seen = true,
            _ if !header_seen => return Err(err("missing the header line".into())),
            _ => {}
        }
        let record = match kind {
            'X' | 'Q' | 'D' => {
                (obj.format.radix, radix) = match kind {
                    'X' => (Radix::Hex, 16),
                    'Q' => (Radix::Octal, 8),
                    _ => (Radix::Decimal, 10),
                };
                // What the linker assumes for flags left out.
                obj.format.big_endian = false;
                obj.format.addr_bytes = 2;
                for c in rest.chars() {
                    match c {
                        'L' => obj.format.big_endian = false,
                        'H' => obj.format.big_endian = true,
                        '2' | '3' | '4' => obj.format.addr_bytes = c as u8 - b'0',
                        _ => return Err(err(format!("unknown format flag '{c}'"))),
                    }
                }
                continue;
            }
            'H' => {
                let fields: Vec<&str> = rest.split_whitespace().collect();
                let [areas, "areas", symbols, "global", "symbols"] = fields[..] else {
                    return Err(err("malformed header".into()));
                };
                Record::Header {
                    areas: num(areas)?,
                    symbols: num(symbols)?,
                }
            }
            'M' => {
                if !valid_area_name(rest) {
                    return Err(err(format!(
                        "the linker reads module name '{rest}' only in part"
                    )));
                }
                Record::Module(rest.to_string())
            }
            'O' => Record::Options(rest.to_string()),
            'A' => {
                // The linker reads the values by position, whatever names
                // them, so only the order the assemblers write is accepted.
                let fields: Vec<&str> = rest.split_whitespace().collect();
                let (name, size, flags, addr) = match fields[..] {
                    [name, "size", size, "flags", flags] => (name, size, flags, None),
                    [name, "size", size, "flags", flags, "addr", addr] => {
                        (name, size, flags, Some(addr))
                    }
                    _ => return Err(err("malformed area line".into())),
                };
                if !valid_area_name(name) {
                    return Err(err(format!(
                        "the linker reads area name '{name}' only in part"
                    )));
                }
                Record::Area {
                    name: name.to_string(),
                    size: num(size)?,
                    flags: num(flags)?,
                    addr: addr.map(num).transpose()?,
                }
            }
            'S' => {
                let fields: Vec<&str> = rest.split_whitespace().collect();
                let [name, kind] = fields[..] else {
                    return Err(err("malformed symbol".into()));
                };
                let (defined, value) = match (kind.strip_prefix("Def"), kind.strip_prefix("Ref")) {
                    (Some(v), _) => (true, num(v)?),
                    (_, Some(v)) => (false, num(v)?),
                    _ => return Err(err(format!("symbol '{name}' is neither Def nor Ref"))),
                };
                Record::Symbol {
                    name: name.to_string(),
                    defined,
                    value,
                }
            }
            'T' => {
                pending = Some((number, bytes(rest, radix, number)?));
                continue;
            }
            'R' | 'P' => {
                let (start, t) = pending
                    .take()
                    .ok_or_else(|| err(format!("{kind} line without a T line")))?;
                let record =
                    text_record(t, kind, &bytes(rest, radix, number)?, obj.format.big_endian)
                        .map_err(err)?;
                obj.records.push(record);
                lines.push(start);
                continue;
            }
            'G' | 'B' => {
                return Err(err("ASxxxx version 4 objects are not supported".into()));
            }
            _ => return Err(err(format!("unsupported record '{kind}'"))),
        };
        obj.records.push(record);
        lines.push(number);
    }
    if let Some((number, _)) = pending {
        return Err(Error::new(number, "T line without an R or P line"));
    }
    if !header_seen {
        let missing = if format_seen { "header" } else { "format" };
        return Err(Error::new(0, format!("missing the {missing} line")));
    }
    Ok((obj, lines))
}

fn bytes(text: &str, radix: u32, line: usize) -> Result<Vec<u8>, Error> {
    text.split_whitespace()
        .map(|t| {
            u8::from_str_radix(t, radix)
                .map_err(|_| Error::new(line, format!("'{t}' is not a byte")))
        })
        .collect()
}

fn text_record(bytes: Vec<u8>, kind: char, rel: &[u8], big_endian: bool) -> Result<Record, String> {
    let word = |b: [u8; 2]| {
        if big_endian {
            u16::from_be_bytes(b)
        } else {
            u16::from_le_bytes(b)
        }
    };
    let [0, 0, a0, a1, ref rest @ ..] = *rel else {
        return Err(format!("malformed {kind} line"));
    };
    let area = word([a0, a1]);
    let mut entries = Vec::new();
    let mut rest = rest;
    while !rest.is_empty() {
        // The linker reads a P line's modes as single bytes.
        let (mode, tail) = match rest {
            [m, low, tail @ ..] if kind == 'R' && u16::from(*m) & ESCAPE == ESCAPE => {
                ((u16::from(*m) & 0x0F) << 8 | u16::from(*low), tail)
            }
            [m, tail @ ..] => (u16::from(*m), tail),
            [] => unreachable!(),
        };
        let [pos, i0, i1, ref tail @ ..] = *tail else {
            return Err(format!("truncated {kind} line entry"));
        };
        entries.push(Entry {
            mode,
            pos,
            index: word([i0, i1]),
        });
        rest = tail;
    }
    Ok(Record::Text {
        bytes,
        kind: if kind == 'R' {
            TextKind::Relocate
        } else {
            TextKind::Page
        },
        area,
        entries,
    })
}

/// Write `obj` in the layout the SDCC assemblers use.
pub fn write(obj: &Object) -> String {
    let f = obj.format;
    let num = |v: u32| match f.radix {
        Radix::Hex => format!("{v:X}"),
        Radix::Octal => format!("{v:o}"),
        Radix::Decimal => format!("{v}"),
    };
    let mut out = String::new();
    let radix = match f.radix {
        Radix::Hex => 'X',
        Radix::Octal => 'Q',
        Radix::Decimal => 'D',
    };
    let endian = if f.big_endian { 'H' } else { 'L' };
    writeln!(out, "{radix}{endian}{}", f.addr_bytes).unwrap();
    for record in &obj.records {
        match record {
            Record::Header { areas, symbols } => writeln!(
                out,
                "H {} areas {} global symbols",
                num(*areas),
                num(*symbols)
            ),
            Record::Module(name) => writeln!(out, "M {name}"),
            Record::Options(text) => writeln!(out, "O {text}"),
            Record::Area {
                name,
                size,
                flags,
                addr,
            } => {
                write!(out, "A {name} size {} flags {}", num(*size), num(*flags)).unwrap();
                if let Some(addr) = addr {
                    write!(out, " addr {}", num(*addr)).unwrap();
                }
                writeln!(out)
            }
            Record::Symbol {
                name,
                defined,
                value,
            } => {
                let kind = if *defined { "Def" } else { "Ref" };
                writeln!(out, "S {name} {kind}{}", symbol_value(*value, f))
            }
            Record::Text {
                bytes,
                kind,
                area,
                entries,
            } => {
                write_bytes(&mut out, 'T', bytes, f.radix);
                let mut rel = vec![0, 0];
                push_word(&mut rel, *area, f.big_endian);
                for e in entries {
                    let escape = *kind == TextKind::Relocate;
                    if escape && (e.mode > 0xFF || e.mode & ESCAPE == ESCAPE) {
                        rel.extend([ESCAPE as u8 | (e.mode >> 8) as u8, e.mode as u8]);
                    } else {
                        rel.push(e.mode as u8);
                    }
                    rel.push(e.pos);
                    push_word(&mut rel, e.index, f.big_endian);
                }
                let line = match kind {
                    TextKind::Relocate => 'R',
                    TextKind::Page => 'P',
                };
                write_bytes(&mut out, line, &rel, f.radix);
                Ok(())
            }
        }
        .unwrap();
    }
    out
}

/// A symbol value, zero-padded to the width of an address.
fn symbol_value(v: u32, f: Format) -> String {
    let digits = |widths: [usize; 3]| widths[usize::from(f.addr_bytes.clamp(2, 4)) - 2];
    match f.radix {
        Radix::Hex => format!("{v:0w$X}", w = digits([4, 6, 8])),
        Radix::Octal => format!("{v:0w$o}", w = digits([6, 8, 11])),
        Radix::Decimal => format!("{v:0w$}", w = digits([5, 8, 10])),
    }
}

fn write_bytes(out: &mut String, kind: char, bytes: &[u8], radix: Radix) {
    out.push(kind);
    for b in bytes {
        match radix {
            Radix::Hex => write!(out, " {b:02X}"),
            Radix::Octal => write!(out, " {b:03o}"),
            Radix::Decimal => write!(out, " {b:03}"),
        }
        .unwrap();
    }
    out.push('\n');
}

fn push_word(buf: &mut Vec<u8>, v: u16, big_endian: bool) {
    let [hi, lo] = v.to_be_bytes();
    buf.extend(if big_endian { [hi, lo] } else { [lo, hi] });
}

#[cfg(test)]
mod tests {
    use super::{Entry, Format, Radix, Record, parse, write};

    #[test]
    fn needs_the_format_then_the_header() {
        for (text, line, message) in [
            ("", 0, "missing the format line"),
            ("H 0 areas 0 global symbols\n", 1, "missing the format line"),
            ("XL4\n", 0, "missing the header line"),
            ("XL4\nM m\n", 2, "missing the header line"),
            (
                "XL4\nXL4\n",
                2,
                "more than one module in a file is not supported",
            ),
        ] {
            let err = parse(text).unwrap_err();
            assert_eq!(
                (err.line, err.message.as_str()),
                (line, message),
                "{text:?}"
            );
        }
    }

    #[test]
    fn area_lines_keep_the_linkers_order() {
        let head = "XL2\nH 1 areas 0 global symbols\n";
        assert!(parse(&format!("{head}A _CODE size 1 flags 0 addr 0\n")).is_ok());
        let err = parse(&format!("{head}A _CODE flags 0 size 1\n")).unwrap_err();
        assert_eq!(err.message, "malformed area line");
    }

    #[test]
    fn area_and_module_names_are_read_whole() {
        let head = "XL2\nH 1 areas 0 global symbols\n";
        for (line, message) in [
            (
                "A _X-Y size 3 flags 0",
                "the linker reads area name '_X-Y' only in part",
            ),
            (
                "M my-module",
                "the linker reads module name 'my-module' only in part",
            ),
        ] {
            let err = parse(&format!("{head}{line}\n")).unwrap_err();
            assert_eq!(err.message, message);
        }
    }

    #[test]
    fn p_line_modes_are_single_bytes() {
        let text = "XL2\nH 1 areas 0 global symbols\nA _CODE size 0 flags 0\n\
                    T 00 00 00 00 00 00\nP 00 00 00 00 F1 02 00 00\n";
        let obj = parse(text).unwrap();
        let Some(Record::Text { entries, .. }) = obj.records.last() else {
            panic!("no P line");
        };
        let entry = Entry {
            mode: 0xF1,
            pos: 2,
            index: 0,
        };
        assert_eq!(entries[..], [entry]);
        assert_eq!(write(&obj), text);
    }

    #[test]
    fn format_flags_default_as_the_linker_has_them() {
        let obj = parse("Q\nH 0 areas 0 global symbols\n").unwrap();
        let expected = Format {
            radix: Radix::Octal,
            big_endian: false,
            addr_bytes: 2,
        };
        assert_eq!(obj.format, expected);
    }
}
