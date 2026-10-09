//! Objects written by the SDCC tools, regenerated from the sources next to
//! them with `sdcc -mz80 -c`, `sdasz80 -o` and `sdasgb -o`.

use asxxxx::{Chunk, Def, Object, Reloc, Symbol, Target, Width, parse, write};

fn load(name: &str) -> Object {
    let path = format!("{}/tests/data/{name}.rel", env!("CARGO_MANIFEST_DIR"));
    let obj = parse(&std::fs::read_to_string(path).unwrap()).unwrap();
    assert_eq!(
        parse(&write(&obj).unwrap()).unwrap(),
        obj,
        "{name} changes when rewritten"
    );
    obj
}

fn reloc(offset: u32, width: Width, target: Target, addend: i64) -> Reloc {
    Reloc {
        offset,
        width,
        pc_relative: false,
        target,
        addend,
    }
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
            reloc(1, Width::AddrHigh, Target::Symbol(0), 300),
            reloc(4, Width::AddrLow, Target::Symbol(0), 300),
            reloc(7, Width::Word, Target::Symbol(1), 0),
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
            reloc(1, Width::AddrLow, hr, 0),
            reloc(3, Width::AddrLow, hr, 1),
            reloc(5, Width::Word, hr, 0),
            Reloc {
                pc_relative: true,
                ..reloc(8, Width::AddrLow, hr, 0)
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
        [reloc(7, Width::Word, Target::Area(1), 0)]
    );
    assert!(obj.areas[1].data.is_empty());
}
