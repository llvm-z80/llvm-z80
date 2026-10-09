use asxxxx::{Chunk, Def, Object as Rel, Symbol};
use object::write::{Object, Relocation, SectionId, Symbol as ElfSymbol, SymbolSection};
use object::{
    Architecture, BinaryFormat, Endianness, RelocationFlags, SectionKind, SymbolFlags, SymbolKind,
    SymbolScope,
};

fn new_object() -> Object<'static> {
    Object::new(BinaryFormat::Elf, Architecture::I386, Endianness::Little)
}

fn add_symbol(obj: &mut Object, name: &str, section: SectionId) {
    obj.add_symbol(ElfSymbol {
        name: name.as_bytes().to_vec(),
        value: 0,
        size: 0,
        kind: SymbolKind::Data,
        scope: SymbolScope::Linkage,
        weak: false,
        section: SymbolSection::Section(section),
        flags: SymbolFlags::None,
    });
}

fn convert(obj: Object) -> Rel {
    let mut elf = obj.write().unwrap();
    // object has no Z80 architecture, so write an i386 ELF32 and retarget it.
    elf[18..20].copy_from_slice(&0x1F90u16.to_le_bytes());
    sdcconv::elf_to_rel(&elf, "m").unwrap().0
}

fn def(name: &str, area: usize, value: u32) -> Symbol {
    Symbol {
        name: name.to_string(),
        def: Some(Def {
            area: Some(area),
            value,
        }),
    }
}

fn chunk(bytes: &[u8]) -> Vec<Chunk> {
    vec![Chunk {
        offset: 0,
        bytes: bytes.to_vec(),
    }]
}

#[test]
fn bss_goes_to_data_without_bytes() {
    let mut obj = new_object();
    let text = obj.add_section(Vec::new(), b".text".to_vec(), SectionKind::Text);
    obj.append_section_data(text, &[0xC9], 1);
    for name in ["_a", "_b"] {
        let bss = obj.add_section(
            Vec::new(),
            format!(".bss.{name}").into_bytes(),
            SectionKind::UninitializedData,
        );
        obj.append_section_bss(bss, 0x800, 1);
        add_symbol(&mut obj, name, bss);
    }
    let rel = convert(obj);
    let areas: Vec<(&str, u32)> = rel
        .areas
        .iter()
        .map(|a| (a.name.as_str(), a.size))
        .collect();
    assert_eq!(areas, [("_CODE", 1), ("_DATA", 0x1000)]);
    assert!(rel.areas[1].data.is_empty());
    assert_eq!(rel.symbols, [def("_a", 1, 0), def("_b", 1, 0x800)]);
}

#[test]
fn sections_go_to_the_areas_sdcc_uses() {
    let mut obj = new_object();
    let text = obj.add_section(Vec::new(), b".text".to_vec(), SectionKind::Text);
    obj.append_section_data(text, &[0xC9], 1);
    let rodata = obj.add_section(Vec::new(), b".rodata".to_vec(), SectionKind::ReadOnlyData);
    obj.append_section_data(rodata, &[1, 2], 1);
    let data = obj.add_section(Vec::new(), b".data".to_vec(), SectionKind::Data);
    obj.append_section_data(data, &[7, 8, 9], 1);
    add_symbol(&mut obj, "_d", data);
    let bss = obj.add_section(Vec::new(), b".bss".to_vec(), SectionKind::UninitializedData);
    obj.append_section_bss(bss, 4, 1);
    let header = obj.add_section(Vec::new(), b"_HEADER".to_vec(), SectionKind::ReadOnlyData);
    obj.append_section_data(header, &[0x42], 1);

    let rel = convert(obj);
    let areas: Vec<(&str, u32)> = rel
        .areas
        .iter()
        .map(|a| (a.name.as_str(), a.size))
        .collect();
    assert_eq!(
        areas,
        [
            ("_CODE", 3),
            ("_DATA", 4),
            ("_INITIALIZED", 3),
            ("_INITIALIZER", 3),
            ("_HEADER", 1),
        ]
    );
    assert_eq!(rel.areas[0].data, chunk(&[0xC9, 1, 2]));
    assert!(rel.areas[2].data.is_empty());
    assert_eq!(rel.areas[3].data, chunk(&[7, 8, 9]));
    assert_eq!(rel.areas[4].data, chunk(&[0x42]));
    assert_eq!(rel.symbols, [def("_d", 2, 0)]);
}

#[test]
fn absolute_and_weak_symbols_carry_over() {
    let mut obj = new_object();
    let text = obj.add_section(Vec::new(), b".text".to_vec(), SectionKind::Text);
    obj.append_section_data(text, &[0xC9], 1);
    for (name, section, weak) in [
        ("_port", SymbolSection::Absolute, false),
        ("_hook", SymbolSection::Section(text), true),
        ("_maybe", SymbolSection::Undefined, true),
    ] {
        obj.add_symbol(ElfSymbol {
            name: name.as_bytes().to_vec(),
            value: if name == "_port" { 0x40 } else { 0 },
            size: 0,
            kind: SymbolKind::Data,
            scope: SymbolScope::Linkage,
            weak,
            section,
            flags: SymbolFlags::None,
        });
    }
    let mut elf = obj.write().unwrap();
    elf[18..20].copy_from_slice(&0x1F90u16.to_le_bytes());
    let (rel, warnings) = sdcconv::elf_to_rel(&elf, "m").unwrap();
    assert_eq!(
        warnings,
        [
            "weak symbol '_hook' becomes an ordinary one",
            "weak symbol '_maybe' becomes an ordinary one"
        ]
    );
    let symbol = |name: &str, def| Symbol {
        name: name.to_string(),
        def,
    };
    assert_eq!(
        rel.symbols,
        [
            symbol(
                "_port",
                Some(Def {
                    area: None,
                    value: 0x40
                })
            ),
            def("_hook", 0, 0),
            symbol("_maybe", None),
        ]
    );
}

#[test]
fn names_a_rel_object_cannot_hold_are_refused() {
    let mut obj = new_object();
    let text = obj.add_section(Vec::new(), b".text".to_vec(), SectionKind::Text);
    obj.append_section_data(text, &[0xC9], 1);
    add_symbol(&mut obj, "_a b", text);
    let mut elf = obj.write().unwrap();
    elf[18..20].copy_from_slice(&0x1F90u16.to_le_bytes());
    let err = sdcconv::elf_to_rel(&elf, "m").unwrap_err();
    assert_eq!(
        err.to_string(),
        "symbol \"_a b\" cannot be named in a .rel object"
    );

    // The linker would read this area as `_X`.
    let mut obj = new_object();
    let text = obj.add_section(Vec::new(), b"_X-Y".to_vec(), SectionKind::Text);
    obj.append_section_data(text, &[0xC9], 1);
    let mut elf = obj.write().unwrap();
    elf[18..20].copy_from_slice(&0x1F90u16.to_le_bytes());
    let err = sdcconv::elf_to_rel(&elf, "m").unwrap_err();
    assert_eq!(
        err.to_string(),
        "section \"_X-Y\" cannot be named in a .rel object"
    );
}

#[test]
fn relocations_without_addends_are_refused() {
    // object writes REL relocations for i386.
    let mut obj = new_object();
    let text = obj.add_section(Vec::new(), b".text".to_vec(), SectionKind::Text);
    obj.append_section_data(text, &[0x21, 0, 0], 1);
    let x = obj.add_symbol(ElfSymbol {
        name: b"_x".to_vec(),
        value: 0,
        size: 0,
        kind: SymbolKind::Data,
        scope: SymbolScope::Linkage,
        weak: false,
        section: SymbolSection::Undefined,
        flags: SymbolFlags::None,
    });
    let flags = RelocationFlags::Elf { r_type: 3 };
    obj.add_relocation(
        text,
        Relocation {
            offset: 1,
            symbol: x,
            addend: 0,
            flags,
        },
    )
    .unwrap();
    let mut elf = obj.write().unwrap();
    elf[18..20].copy_from_slice(&0x1F90u16.to_le_bytes());
    let err = sdcconv::elf_to_rel(&elf, "m").unwrap_err();
    assert_eq!(
        err.to_string(),
        "relocations without explicit addends are not supported"
    );
}

#[test]
fn sections_named_after_sdcc_areas_are_refused() {
    for name in ["_INITIALIZER", "_CABS"] {
        let mut obj = new_object();
        let text = obj.add_section(Vec::new(), name.as_bytes().to_vec(), SectionKind::Data);
        obj.append_section_data(text, &[1], 1);
        let mut elf = obj.write().unwrap();
        elf[18..20].copy_from_slice(&0x1F90u16.to_le_bytes());
        let err = sdcconv::elf_to_rel(&elf, "m").unwrap_err();
        let message = format!("section {name:?} takes the name of an area SDCC reserves");
        assert_eq!(err.to_string(), message);
    }
}

#[test]
fn module_names_keep_only_what_the_linker_reads() {
    let mut obj = new_object();
    let text = obj.add_section(Vec::new(), b".text".to_vec(), SectionKind::Text);
    obj.append_section_data(text, &[0xC9], 1);
    let mut elf = obj.write().unwrap();
    elf[18..20].copy_from_slice(&0x1F90u16.to_le_bytes());
    let rel = sdcconv::elf_to_rel(&elf, "my-file 2").unwrap().0;
    assert_eq!(rel.name.as_deref(), Some("my_file_2"));
}

#[test]
fn areas_must_fit_an_address_space() {
    let mut obj = new_object();
    let bss = obj.add_section(Vec::new(), b".bss".to_vec(), SectionKind::UninitializedData);
    obj.append_section_bss(bss, 0x100_0001, 1);
    let mut elf = obj.write().unwrap();
    elf[18..20].copy_from_slice(&0x1F90u16.to_le_bytes());
    let err = sdcconv::elf_to_rel(&elf, "m").unwrap_err();
    assert_eq!(
        err.to_string(),
        ".bss makes _DATA larger than any Z80 address space"
    );
}

#[test]
fn common_symbols_are_refused() {
    let mut obj = new_object();
    obj.add_symbol(ElfSymbol {
        name: b"_c".to_vec(),
        value: 1,
        size: 2,
        kind: SymbolKind::Data,
        scope: SymbolScope::Linkage,
        weak: false,
        section: SymbolSection::Common,
        flags: SymbolFlags::None,
    });
    let mut elf = obj.write().unwrap();
    elf[18..20].copy_from_slice(&0x1F90u16.to_le_bytes());
    let err = sdcconv::elf_to_rel(&elf, "m").unwrap_err();
    assert!(err.to_string().contains("-fno-common"), "{err}");
}
