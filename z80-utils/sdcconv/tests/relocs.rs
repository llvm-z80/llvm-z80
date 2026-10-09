use asxxxx::{Reloc, Target, Width};
use object::read::elf::ElfFile32;
use object::write::{Object, Relocation, Symbol, SymbolSection};
use object::{
    Architecture, BinaryFormat, Endianness, LittleEndian, Object as _, ObjectSection, ObjectSymbol,
    RelocationFlags, RelocationTarget, SectionKind, SymbolFlags, SymbolKind, SymbolScope,
};

const R_Z80_ADDR16: u32 = 3;
const R_Z80_ADDR16_LO: u32 = 4;
const R_Z80_ADDR16_HI: u32 = 5;
const R_Z80_PCREL_8: u32 = 6;
const R_Z80_PCREL_16: u32 = 12;

fn reloc(offset: u32, width: Width, pc_relative: bool, addend: i64) -> Reloc {
    Reloc {
        offset,
        width,
        pc_relative,
        target: Target::Symbol(0),
        addend,
    }
}

#[test]
fn address_bytes_keep_their_half_and_addend() {
    // object writes REL for i386, so take another ELF32 target that uses RELA.
    let mut obj = Object::new(BinaryFormat::Elf, Architecture::Riscv32, Endianness::Little);
    let text = obj.add_section(Vec::new(), b".text".to_vec(), SectionKind::Text);
    obj.append_section_data(text, &[0x3E, 0, 0xC9, 0x3E, 0, 0xC9, 0x18, 0], 1);
    let tbl = obj.add_symbol(Symbol {
        name: b"_tbl".to_vec(),
        value: 0,
        size: 0,
        kind: SymbolKind::Data,
        scope: SymbolScope::Linkage,
        weak: false,
        section: SymbolSection::Undefined,
        flags: SymbolFlags::None,
    });
    for (offset, r_type, addend) in [
        (1, R_Z80_ADDR16_HI, 300),
        (4, R_Z80_ADDR16_LO, 300),
        (7, R_Z80_PCREL_8, 0),
    ] {
        obj.add_relocation(
            text,
            Relocation {
                offset,
                symbol: tbl,
                addend,
                flags: RelocationFlags::Elf { r_type },
            },
        )
        .unwrap();
    }
    let mut elf = obj.write().unwrap();
    elf[18..20].copy_from_slice(&0x1F90u16.to_le_bytes());

    let rel = sdcconv::elf_to_rel(&elf, "m").unwrap();
    assert_eq!(
        rel.areas[0].relocs,
        [
            reloc(1, Width::AddrHigh, false, 300),
            reloc(4, Width::AddrLow, false, 300),
            reloc(7, Width::Byte, true, 0),
        ]
    );
}

/// What sdasz80 writes for `ld a, #>(_tbl + 300)`, `ld a, #<(_tbl + 300)`,
/// `jr _tbl`, `ld hl, #_tbl` and a PC-relative word.
const REL: &str = "\
XL4
H 1 areas 2 global symbols
M m
S .__.ABS. Def00000000
S _tbl Ref00000000
A _CODE size B flags 0 addr 0
T 00 00 00 00 3E 2C 01 00 00 3E 2C 01 00 00
R 00 00 00 00 8B 05 01 00 0B 0A 01 00
T 04 00 00 00 18 00 00 00 00 21 00 00 00 00
R 00 00 00 00 0F 05 01 00 02 0A 01 00 06 0C 01 00
";

#[test]
fn sdas_relocations_become_lld_ones() {
    let elf = sdcconv::rel_to_elf(&asxxxx::parse(REL).unwrap()).unwrap();
    let elf = ElfFile32::<LittleEndian>::parse(&*elf).unwrap();
    let text = elf.section_by_name(".text").unwrap();
    let relocs: Vec<(u64, u32, i64, String)> = text
        .relocations()
        .map(|(offset, r)| {
            let RelocationFlags::Elf { r_type } = r.flags() else {
                unreachable!()
            };
            let RelocationTarget::Symbol(s) = r.target() else {
                unreachable!()
            };
            let name = elf.symbol_by_index(s).unwrap().name().unwrap().to_string();
            (offset, r_type, r.addend(), name)
        })
        .collect();
    let tbl = || "_tbl".to_string();
    assert_eq!(
        relocs,
        [
            (1, R_Z80_ADDR16_HI, 300, tbl()),
            (3, R_Z80_ADDR16_LO, 300, tbl()),
            (5, R_Z80_PCREL_8, 0, tbl()),
            (7, R_Z80_ADDR16, 0, tbl()),
            (9, R_Z80_PCREL_16, -1, tbl()),
        ]
    );
}

#[test]
fn relocations_survive_a_round_trip() {
    let rel = asxxxx::parse(REL).unwrap();
    let elf = sdcconv::rel_to_elf(&rel).unwrap();
    let back = sdcconv::elf_to_rel(&elf, "m").unwrap();
    let mut expected = rel.areas[0].relocs.clone();
    // ELF has one PC-relative byte, however sdas wrote the field.
    expected[2].width = Width::Byte;
    assert_eq!(back.areas[0].relocs, expected);
    assert_eq!(back.areas[0].data, rel.areas[0].data);
}
