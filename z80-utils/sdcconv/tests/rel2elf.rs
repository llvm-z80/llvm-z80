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
    let elf = sdcconv::rel_to_elf(&asxxxx::parse(REL).unwrap()).unwrap().0;
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
    let elf = sdcconv::rel_to_elf(&asxxxx::parse(rel).unwrap()).unwrap().0;
    let back = sdcconv::elf_to_rel(&elf, "m").unwrap().0;
    let area_name = |a: Option<usize>| a.map(|a| back.areas[a].name.as_str());
    assert_eq!(area_name(back.symbols[0].def.unwrap().area), Some("_DATA"));
    let targets: Vec<_> = back.areas[0]
        .relocs
        .iter()
        .filter_map(|r| match r.target {
            asxxxx::Target::Area(a) => area_name(Some(a)),
            _ => None,
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
    let elf = sdcconv::rel_to_elf(&asxxxx::parse(rel).unwrap()).unwrap().0;
    let back = sdcconv::elf_to_rel(&elf, "m").unwrap().0;
    let names: Vec<&str> = back.areas.iter().map(|a| a.name.as_str()).collect();
    assert_eq!(names, ["_.note", "_.text.home"]);
}

#[test]
fn objects_no_z80_can_hold_are_refused() {
    for (format, areas, message) in [
        (
            "XL4",
            "A _CODE size 800000 flags 0 addr 0\nA _DATA size 800001 flags 0 addr 0\n",
            "the areas add up to more than any Z80 address space",
        ),
        (
            "XL4",
            "A _CABS size 1 flags 8 addr 100\nS _x Def00000080\n",
            "'_x' lies before its absolute area",
        ),
        (
            "XL4",
            "A _OSEG size 4 flags 4 addr 0\n",
            "_OSEG has area flags 4 an ELF object cannot hold",
        ),
        (
            "XL4",
            "A _PSEG size 2 flags 10 addr 0\n",
            "_PSEG has area flags 10 an ELF object cannot hold",
        ),
        ("XH4", "", "a big-endian object is not a Z80 one"),
    ] {
        let rel = format!("{format}\nH 2 areas 2 global symbols\nS .__.ABS. Def00000000\n{areas}");
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

#[test]
fn a_member_that_cannot_be_converted_fails_the_library() {
    use ar_archive_writer::{ArchiveKind, DEFAULT_OBJECT_READER, NewArchiveMember};
    let big_endian = "XH4\nH 1 areas 0 global symbols\nA _CODE size 0 flags 0 addr 0\n";
    let members: Vec<NewArchiveMember> = [("good.rel", REL), ("bad.rel", big_endian)]
        .into_iter()
        .map(|(name, text)| NewArchiveMember {
            buf: Box::new(text.as_bytes().to_vec()),
            object_reader: &DEFAULT_OBJECT_READER,
            member_name: name.to_string(),
            mtime: 0,
            uid: 0,
            gid: 0,
            perms: 0o644,
        })
        .collect();
    let mut lib = std::io::Cursor::new(Vec::new());
    ar_archive_writer::write_archive_to_stream(&mut lib, &members, ArchiveKind::Gnu, false, None)
        .unwrap();
    let err = sdcconv::lib_to_elf_archive(&lib.into_inner(), false).unwrap_err();
    assert_eq!(
        err.to_string(),
        "bad.rel: a big-endian object is not a Z80 one"
    );
}

#[test]
fn absolute_areas_are_left_to_the_linker_script() {
    let rel = "\
XL4
H 1 areas 2 global symbols
S .__.ABS. Def00000000
A _CABS0 size 2 flags 8 addr 100
S _tag Def00000100
T 00 01 00 00 01 02
R 00 00 00 00
";
    let (_, warnings) = sdcconv::rel_to_elf(&asxxxx::parse(rel).unwrap()).unwrap();
    assert_eq!(
        warnings,
        ["_CABS0 must be placed at 0x0100 by the linker script"]
    );
}

#[test]
fn zeros_in_an_area_keep_their_bytes() {
    let rel = "\
XL4
H 2 areas 1 global symbols
S .__.ABS. Def00000000
A _TABLE size 4 flags 0 addr 0
A _SPACE size 4 flags 0 addr 0
T 00 00 00 00 00 00 00 00
R 00 00 00 00
";
    let (elf, _) = sdcconv::rel_to_elf(&asxxxx::parse(rel).unwrap()).unwrap();
    let elf = ElfFile32::<LittleEndian>::parse(&*elf).unwrap();
    let types: Vec<(&str, u32)> = ["_TABLE", "_SPACE"]
        .into_iter()
        .map(|name| {
            let section = elf.section_by_name(name).unwrap();
            (name, section.elf_section_header().sh_type.get(LittleEndian))
        })
        .collect();
    assert_eq!(types, [("_TABLE", SHT_PROGBITS), ("_SPACE", SHT_NOBITS)]);
}
