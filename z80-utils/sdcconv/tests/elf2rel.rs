use asxxxx::{Chunk, Def, Symbol};
use object::write::{Object, Symbol as ElfSymbol, SymbolSection};
use object::{
    Architecture, BinaryFormat, Endianness, SectionKind, SymbolFlags, SymbolKind, SymbolScope,
};

/// An ELF object with one byte of code and two 2 KB uninitialized globals.
fn bss_object() -> Vec<u8> {
    let mut obj = Object::new(BinaryFormat::Elf, Architecture::I386, Endianness::Little);
    let text = obj.add_section(Vec::new(), b".text".to_vec(), SectionKind::Text);
    obj.append_section_data(text, &[0xC9], 1);
    for name in ["_a", "_b"] {
        let bss = obj.add_section(
            Vec::new(),
            format!(".bss.{name}").into_bytes(),
            SectionKind::UninitializedData,
        );
        obj.append_section_bss(bss, 0x800, 1);
        obj.add_symbol(ElfSymbol {
            name: name.as_bytes().to_vec(),
            value: 0,
            size: 0x800,
            kind: SymbolKind::Data,
            scope: SymbolScope::Linkage,
            weak: false,
            section: SymbolSection::Section(bss),
            flags: SymbolFlags::None,
        });
    }
    let mut elf = obj.write().unwrap();
    // object has no Z80 architecture, so write an i386 ELF32 and retarget it.
    elf[18..20].copy_from_slice(&0x1F90u16.to_le_bytes());
    elf
}

#[test]
fn bss_takes_its_own_area_without_bytes() {
    let rel = sdcconv::elf_to_rel(&bss_object(), "bss").unwrap();
    let areas: Vec<(&str, u32)> = rel
        .areas
        .iter()
        .map(|a| (a.name.as_str(), a.size))
        .collect();
    assert_eq!(areas, [("_CODE", 1), ("_BSS", 0x1000)]);
    assert_eq!(
        rel.areas[0].data,
        [Chunk {
            offset: 0,
            bytes: vec![0xC9]
        }]
    );
    assert!(rel.areas[1].data.is_empty());
    let def = |name: &str, value| Symbol {
        name: name.to_string(),
        def: Some(Def {
            area: Some(1),
            value,
        }),
    };
    assert_eq!(rel.symbols, [def("_a", 0), def("_b", 0x800)]);
}
