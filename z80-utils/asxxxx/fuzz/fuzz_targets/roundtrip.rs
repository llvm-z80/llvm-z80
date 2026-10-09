//! Whatever parses must write, and read back the same.

#![no_main]

use asxxxx::{Linker, parse_as, raw, write};
use libfuzzer_sys::fuzz_target;

fuzz_target!(|data: &[u8]| {
    let Ok(text) = std::str::from_utf8(data) else {
        return;
    };
    if let Ok(obj) = raw::parse(text) {
        assert_eq!(raw::parse(&raw::write(&obj)), Ok(obj));
    }
    for linker in [Linker::Standard, Linker::Pdk] {
        if let Ok(obj) = parse_as(text, linker) {
            let text = write(&obj).expect("a parsed object should write");
            assert_eq!(parse_as(&text, linker), Ok(obj));
        }
    }
});
