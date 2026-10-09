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
