//! Objects written by the SDCC tools, regenerated from the sources next to
//! them with `sdcc -mz80 -c` or the assembler for their port, such as
//! `sdas8051 -o mcs51.rel mcs51.s`.

use asxxxx::{
    Chunk, Def, Field, Linker, Object, Page, PageBase, PdkPart, Reloc, Symbol, Target, parse_as,
    write,
};

fn load(name: &str) -> Object {
    load_as(name, Linker::Standard)
}

fn load_as(name: &str, linker: Linker) -> Object {
    let path = format!("{}/tests/data/{name}.rel", env!("CARGO_MANIFEST_DIR"));
    let obj = parse_as(&std::fs::read_to_string(path).unwrap(), linker).unwrap();
    assert_eq!(
        parse_as(&write(&obj).unwrap(), linker).unwrap(),
        obj,
        "{name} changes when rewritten"
    );
    obj
}

fn reloc(offset: u32, field: Field, target: Target, addend: i64) -> Reloc {
    Reloc {
        offset,
        field,
        pc_relative: false,
        unsigned: false,
        page: Page::None,
        target,
        addend,
    }
}

fn pc(r: Reloc) -> Reloc {
    Reloc {
        pc_relative: true,
        ..r
    }
}

fn page_zero(page: Option<usize>, r: Reloc) -> Reloc {
    Reloc {
        page: Page::Zero(page),
        ..r
    }
}

fn long(index: u8, bytx: bool) -> Field {
    Field::LongByte { index, bytx }
}

fn symbol(name: &str, def: Option<(Option<usize>, u32)>) -> Symbol {
    Symbol {
        name: name.to_string(),
        def: def.map(|(area, value)| Def { area, value }),
    }
}

#[test]
fn sdcc() {
    let obj = load("sdcc");
    assert_eq!(obj.name.as_deref(), Some("sdcc"));
    assert_eq!(obj.options, ["-mz80 sdcccall(1)"]);
    let names: Vec<&str> = obj.areas.iter().map(|a| a.name.as_str()).collect();
    assert_eq!(
        names,
        [
            "_CODE",
            "_DATA",
            "_INITIALIZED",
            "_DABS",
            "_HOME",
            "_GSINIT",
            "_GSFINAL",
            "_INITIALIZER",
            "_CABS"
        ]
    );
    assert!(obj.areas[3].is_absolute() && !obj.areas[0].is_absolute());
    assert_eq!(
        obj.symbols,
        [
            symbol("_tbl", None),
            symbol("_ext", None),
            symbol("_tail", Some((Some(0), 6))),
            symbol("_hi", Some((Some(0), 0))),
            symbol("_lo", Some((Some(0), 3))),
            symbol("_g", Some((Some(2), 0))),
        ]
    );

    let code = &obj.areas[0];
    assert_eq!(
        code.data,
        [Chunk {
            offset: 0,
            bytes: vec![0x3E, 0, 0xC9, 0x3E, 0, 0xC9, 0xC3, 0, 0],
        }]
    );
    assert_eq!(
        code.relocs,
        [
            reloc(1, Field::AddrByte(1), Target::Symbol(0), 300),
            reloc(4, Field::AddrByte(0), Target::Symbol(0), 300),
            reloc(7, Field::Word, Target::Symbol(1), 0),
        ]
    );
    assert_eq!(obj.areas[7].data[0].bytes, [3]);
}

#[test]
fn sm83() {
    let obj = load("sm83");
    let code = &obj.areas[0];
    assert_eq!(
        code.data,
        [Chunk {
            offset: 0,
            bytes: vec![0xE0, 0, 0xF0, 0, 0xFA, 0, 0, 0x18, 0],
        }]
    );
    let hr = Target::Symbol(0);
    assert_eq!(
        code.relocs,
        [
            reloc(1, Field::AddrByte(0), hr, 0),
            reloc(3, Field::AddrByte(0), hr, 1),
            reloc(5, Field::Word, hr, 0),
            Reloc {
                pc_relative: true,
                ..reloc(8, Field::AddrByte(0), hr, 0)
            },
        ]
    );
}

#[test]
fn absolute() {
    let obj = load("absolute");
    assert_eq!(
        obj.symbols,
        [
            symbol("port", Some((None, 0x40))),
            symbol("v", Some((Some(1), 0)))
        ]
    );
    assert_eq!(
        obj.areas[0].relocs,
        [reloc(7, Field::Word, Target::Area(1), 0)]
    );
    assert!(obj.areas[1].data.is_empty());
}

#[test]
fn z80() {
    let obj = load("z80");
    let ext = Target::Symbol(0);
    assert_eq!(
        obj.areas[0].relocs[..3],
        [
            pc(reloc(1, Field::AddrByte(0), ext, 0)),
            pc(reloc(3, Field::AddrByte(0), ext, 0)),
            pc(reloc(5, Field::AddrByte(0), Target::Area(1), 0)),
        ]
    );
    assert_eq!(
        obj.areas[1].relocs,
        [
            pc(reloc(1, Field::AddrByte(0), Target::Area(0), 0)),
            reloc(3, Field::Word, Target::Area(0), 0),
        ]
    );
}

#[test]
fn mcs51() {
    let obj = load("mcs51");
    let code = &obj.areas[1];
    let (bit, ext) = (Target::Symbol(0), Target::Symbol(1));
    assert_eq!(
        code.relocs[..3],
        [
            reloc(0, Field::Jump11 { opcode: 0x01 }, ext, 0),
            reloc(2, Field::Jump11 { opcode: 0x11 }, ext, 0x10),
            pc(reloc(5, long(0, false), ext, 0)),
        ]
    );
    assert_eq!(
        code.relocs[6],
        page_zero(None, reloc(0xE, long(0, false), bit, 0))
    );
    assert_eq!(
        code.relocs[9..11],
        [
            reloc(0x15, long(0, true), ext, 0),
            reloc(0x17, long(1, true), ext, 0)
        ]
    );
    // A bit of a bit-addressable byte carries its number above the address.
    assert_eq!(
        obj.areas[4].relocs[0],
        page_zero(None, reloc(1, long(0, false), Target::Area(3), 0x8300))
    );
}

#[test]
fn ds390() {
    let code = &load("ds390").areas[1];
    let ext = Target::Symbol(0);
    assert_eq!(
        code.relocs[..2],
        [
            reloc(1, Field::Word24, ext, 0),
            reloc(5, Field::Word24, ext, 0x10000)
        ]
    );
    assert_eq!(
        code.relocs[4..7],
        [
            reloc(0xE, long(0, true), ext, 0),
            reloc(0xF, long(1, true), ext, 0),
            reloc(0x10, long(2, false), ext, 0),
        ]
    );
}

#[test]
fn hc08() {
    let obj = load("hc08");
    // The `.setdp` that makes DIRECT the page of the accesses after it.
    assert_eq!(
        obj.pages,
        [PageBase {
            area: 1,
            bytes: vec![0, 0x17, 0, 2, 0, 0],
            relocs: vec![],
        }]
    );
    let code = &obj.areas[1];
    assert_eq!(
        code.relocs[2],
        page_zero(None, reloc(5, Field::AddrByte(0), Target::Symbol(0), 0))
    );
    assert_eq!(
        code.relocs.last(),
        Some(&page_zero(
            Some(0),
            reloc(0x18, Field::AddrByte(0), Target::Area(2), 0)
        ))
    );
}

#[test]
fn stm8() {
    let code = &load("stm8").areas[1];
    let ext = Target::Symbol(0);
    assert_eq!(code.relocs[0], pc(reloc(1, long(0, false), ext, 0)));
    assert_eq!(code.relocs[2], reloc(5, Field::Word24, ext, 0));
    assert_eq!(
        code.relocs[5],
        Reloc {
            unsigned: true,
            ..reloc(0x11, Field::Word, ext, 0)
        }
    );
}

#[test]
fn mos6502() {
    let code = &load("mos6502").areas[1];
    assert_eq!(
        code.relocs[1..3],
        [
            pc(reloc(3, Field::AddrByte(0), Target::Area(2), 0)),
            page_zero(None, reloc(5, Field::AddrByte(0), Target::Symbol(0), 0)),
        ]
    );
}

#[test]
fn rabbit() {
    let code = &load("rabbit").areas[0];
    let ext = Target::Symbol(0);
    // The linker misplaces the second of these, which shares a line with
    // the first; the model keeps what the assembler meant.
    assert_eq!(
        code.relocs[..2],
        [
            pc(reloc(1, long(0, false), ext, 0)),
            pc(reloc(3, long(0, false), ext, 0))
        ]
    );
}

#[test]
fn tlcs90() {
    let code = &load("tlcs90").areas[0];
    assert_eq!(
        code.relocs[1],
        pc(reloc(3, Field::AddrByte(0), Target::Symbol(0), 0))
    );
}

#[test]
fn f8() {
    let code = &load("f8").areas[1];
    assert_eq!(
        code.relocs[0],
        pc(reloc(1, Field::AddrByte(0), Target::Symbol(0), -1))
    );
}

#[test]
fn pdk14() {
    // sdaspdk14 records no options, so nothing names the linker.
    let code = &load_as("pdk14", Linker::Pdk).areas[1];
    let ext = Target::Symbol(0);
    let pdk = |part, opcode| Field::Pdk {
        part,
        opcode,
        version: 14,
    };
    assert_eq!(
        code.relocs[0],
        reloc(0, pdk(PdkPart::Whole, 0x3000), ext, 0)
    );
    assert_eq!(
        code.relocs[5..],
        [
            reloc(0xA, pdk(PdkPart::Low, 0x200), ext, 0),
            reloc(0xC, pdk(PdkPart::High, 0x200), ext, 0),
        ]
    );
}
