//! Whatever rel2elf accepts must convert back with elf2rel, and the result
//! must survive a second round trip unchanged.

#![no_main]

use asxxxx::{Chunk, Object};
use libfuzzer_sys::fuzz_target;

/// `obj` with each area's gaps filled with zeros, since an ELF section cannot
/// leave any.
fn filled(mut obj: Object) -> Object {
    for area in &mut obj.areas {
        let mut bytes = vec![0; area.size as usize];
        for chunk in &area.data {
            bytes[chunk.offset as usize..chunk.end() as usize].copy_from_slice(&chunk.bytes);
        }
        area.data = vec![Chunk { offset: 0, bytes }];
    }
    obj
}

fuzz_target!(|data: &[u8]| {
    let Ok(text) = std::str::from_utf8(data) else {
        return;
    };
    let Ok(rel) = asxxxx::parse(text) else {
        return;
    };
    let Ok(elf) = sdcconv::rel_to_elf(&rel) else {
        return;
    };
    let once = sdcconv::elf_to_rel(&elf, "m").expect("rel2elf output should convert back");
    let elf = sdcconv::rel_to_elf(&once).expect("elf2rel output should convert again");
    let twice = sdcconv::elf_to_rel(&elf, "m").unwrap();
    assert_eq!(filled(once), filled(twice));
});
