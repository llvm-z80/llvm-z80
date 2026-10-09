//! Whatever elf2rel accepts must write as a `.rel` object that reads back the
//! same.

#![no_main]

use libfuzzer_sys::fuzz_target;

fuzz_target!(|data: &[u8]| {
    let Ok((obj, _)) = sdcconv::elf_to_rel(data, "m") else {
        return;
    };
    let text = asxxxx::write(&obj).expect("elf2rel output should write");
    let back = asxxxx::parse(&text).expect("elf2rel output should read back");
    assert_eq!(asxxxx::write(&back).unwrap(), text);
});
