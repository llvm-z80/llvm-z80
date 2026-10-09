use object::elf::{SHF_ALLOC, SHF_EXECINSTR, SHF_WRITE, SHT_NOBITS, SHT_PROGBITS};
use object::read::elf::ElfFile32;
use object::{LittleEndian, Object, ObjectSection, ObjectSymbol, SectionFlags};

const REL: &str = "\
XL4
H 6 areas 4 global symbols
M m
S .__.ABS. Def00000000
A _CODE size 1 flags 0 addr 0
A _INITIALIZED size 2 flags 0 addr 0
S _g Def00000000
A _DATA size 3 flags 0 addr 0
S _z Def00000000
A _BSS size 1 flags 0 addr 0
A _INITIALIZER size 2 flags 0 addr 0
A _HRAM size 1 flags 0 addr 0
S _h Def00000000
T 00 00 00 00 C9
R 00 00 00 00
T 00 00 00 00 34 12
R 00 00 04 00
";

#[test]
fn areas_become_the_sections_their_contents_need() {
    let elf = sdcconv::rel_to_elf(&asxxxx::parse(REL).unwrap()).unwrap();
    let elf = ElfFile32::<LittleEndian>::parse(&*elf).unwrap();
    let sections: Vec<(&str, u32, u64, Vec<u8>)> = elf
        .sections()
        .filter_map(|s| {
            let SectionFlags::Elf { sh_flags } = s.flags() else {
                unreachable!()
            };
            if sh_flags & u64::from(SHF_ALLOC) == 0 {
                return None;
            }
            let header = s.elf_section_header();
            let sh_type = header.sh_type.get(LittleEndian);
            Some((
                s.name().unwrap(),
                sh_type,
                sh_flags,
                s.data().unwrap().to_vec(),
            ))
        })
        .collect();
    let code = u64::from(SHF_ALLOC | SHF_EXECINSTR);
    let data = u64::from(SHF_ALLOC | SHF_WRITE);
    assert_eq!(
        sections,
        [
            (".text", SHT_PROGBITS, code, vec![0xC9]),
            (".data", SHT_PROGBITS, data, vec![0x34, 0x12]),
            (".bss", SHT_NOBITS, data, vec![]),
            (".bss", SHT_NOBITS, data, vec![]),
            ("_HRAM", SHT_NOBITS, data, vec![]),
        ]
    );
    let in_section = |name: &str| {
        let symbol = elf.symbol_by_name(name).unwrap();
        let section = elf
            .section_by_index(symbol.section_index().unwrap())
            .unwrap();
        (section.name().unwrap().to_string(), symbol.address())
    };
    assert_eq!(in_section("_g"), (".data".into(), 0));
    assert_eq!(in_section("_h"), ("_HRAM".into(), 0));
}

#[test]
fn initialized_halves_must_match() {
    let rel = REL.replace("A _INITIALIZER size 2", "A _INITIALIZER size 3");
    let err = sdcconv::rel_to_elf(&asxxxx::parse(&rel).unwrap()).unwrap_err();
    assert_eq!(
        err.to_string(),
        "_INITIALIZED and _INITIALIZER differ in size"
    );
}

#[test]
fn libc_renaming_is_asked_for() {
    let rel = "\
XL4
H 1 areas 3 global symbols
S .__.ABS. Def00000000
S _strcmp Ref00000000
A _CODE size 1 flags 0 addr 0
S _memset Def00000000
T 00 00 00 00 C9
R 00 00 00 00
";
    let mut obj = asxxxx::parse(rel).unwrap();
    let names = |obj: &asxxxx::Object| -> Vec<String> {
        obj.symbols.iter().map(|s| s.name.clone()).collect()
    };
    assert_eq!(names(&obj), ["_strcmp", "_memset"]);
    sdcconv::rename_libc_symbols(&mut obj);
    assert_eq!(names(&obj), ["_strcmp__sdcc", "_memset__sdcc"]);
}

#[test]
fn empty_areas_keep_what_refers_to_them() {
    // A label in an empty _DATA, and words against that area and an
    // _INITIALIZED that has no _INITIALIZER.
    let rel = "\
XL4
H 3 areas 2 global symbols
S .__.ABS. Def00000000
A _CODE size 6 flags 0 addr 0
A _DATA size 0 flags 0 addr 0
S _s Def00000000
A _INITIALIZED size 0 flags 0 addr 0
T 00 00 00 00 00 00 00 00 00 00
R 00 00 00 00 02 04 01 00 00 06 01 00 00 08 02 00
";
    let elf = sdcconv::rel_to_elf(&asxxxx::parse(rel).unwrap()).unwrap();
    let back = sdcconv::elf_to_rel(&elf, "m").unwrap();
    let area_name = |a: Option<usize>| a.map(|a| back.areas[a].name.as_str());
    assert_eq!(area_name(back.symbols[0].def.unwrap().area), Some("_DATA"));
    let targets: Vec<_> = back.areas[0]
        .relocs
        .iter()
        .filter_map(|r| match r.target {
            asxxxx::Target::Area(a) => area_name(Some(a)),
            asxxxx::Target::Symbol(_) => None,
        })
        .collect();
    assert_eq!(targets, ["_DATA", "_INITIALIZED"]);
}

#[test]
fn other_areas_keep_their_name_through_elf() {
    // Without a leading `_`, elf2rel would drop the first and read the
    // second as _HOME.
    let rel = "\
XL4
H 2 areas 1 global symbols
S .__.ABS. Def00000000
A .note size 1 flags 0 addr 0
A .text.home size 1 flags 0 addr 0
T 00 00 00 00 01
R 00 00 00 00
T 00 00 00 00 02
R 00 00 01 00
";
    let elf = sdcconv::rel_to_elf(&asxxxx::parse(rel).unwrap()).unwrap();
    let back = sdcconv::elf_to_rel(&elf, "m").unwrap();
    let names: Vec<&str> = back.areas.iter().map(|a| a.name.as_str()).collect();
    assert_eq!(names, ["_.note", "_.text.home"]);
}

#[test]
fn areas_must_fit_an_address_space() {
    for (areas, message) in [
        (
            "A _DATA size 1000001 flags 0 addr 0\n",
            "_DATA is larger than any Z80 address space",
        ),
        (
            "A _CABS size 1 flags 8 addr 100\nS _x Def00000080\n",
            "'_x' lies before its absolute area",
        ),
    ] {
        let rel = format!("XL4\nH 1 areas 2 global symbols\nS .__.ABS. Def00000000\n{areas}");
        let err = sdcconv::rel_to_elf(&asxxxx::parse(&rel).unwrap()).unwrap_err();
        assert_eq!(err.to_string(), message);
    }
}

#[test]
fn other_ports_relocations_are_refused() {
    // One byte of a 24-bit address, as the 8051 assembler writes it.
    let rel = "\
XL3
H 1 areas 2 global symbols
S .__.ABS. Def000000
S _x Ref000000
A CSEG size 1 flags 0 addr 0
T 00 00 00 00 00 00
R 00 00 00 00 F1 03 03 01 00
";
    let err = sdcconv::rel_to_elf(&asxxxx::parse(rel).unwrap()).unwrap_err();
    assert_eq!(err.to_string(), "unsupported relocation at 0x0000 in CSEG");
}
