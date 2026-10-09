# asxxxx

Library for reading and writing ASxxxx object files (`.rel`), the object
format of the SDCC assemblers and linkers. It covers the version 3 objects of
every SDCC port, as the `sdas` assemblers write them and the `sdld` linkers
read them.

## Two layers

`asxxxx::raw` keeps a file's records as they are: the format line, header,
module, options, areas, symbols, and each `T` line with the `R` or `P` line
that follows it. It interprets nothing beyond the line structure and writes the
records back in the layout the assemblers use.

`parse` and `write` work on `Object`, which holds each area's contents the way
the linker places them and every relocation as a typed field (`Field`) with an
addend. The contents hold zeros where a field goes.

```rust
let obj = asxxxx::parse(&text)?;
let text = asxxxx::write(&obj)?;
```

## Reading as sdld does

Relocation fields are read following the tests in sdld's `lkrloc3.c`,
including the SDCC extensions: 24-bit fields, 8051 bit addresses and 11-bit
jumps, and Padauk instruction fusion. The Padauk linker reads every relocation
differently from the others. `parse` recognises a Padauk object by the options
SDCC records in its `O` line; an object assembled by hand has none and needs
`parse_as(text, Linker::Pdk)`.

Input that sdld would reject, or would read differently from the model, is
refused, so that writing an object back cannot change how it links. This
covers, among others:

- more than one module in a file
- a `T` line of more than 16 values, or contents outside their area or the
  address space
- area and module names with characters sdld does not read as part of a name
- area attributes or relocation entries out of the order sdld reads them in
- a `.__.ABS.` symbol that is not defined as zero ahead of the areas
- relocation modes sdld cannot fill in, such as the DS80C390 19-bit jump

Version 4 objects (`G` and `B` records), which the SDCC assemblers never write,
are not supported.

## Tests

```bash
cargo test -p asxxxx
```

`tests/data` holds objects written by the SDCC tools, each next to the source
it was made from. Regenerate one with the assembler for its port, for example
`sdas8051 -o mcs51.rel mcs51.s`, or with `sdcc -mz80 -c sdcc.c` for the C file.

## Fuzzing

`fuzz/` holds a [cargo-fuzz](https://github.com/rust-fuzz/cargo-fuzz) target
that checks that whatever parses also writes and reads back the same. It needs
a nightly toolchain.

```bash
cargo install cargo-fuzz
cd asxxxx
mkdir -p fuzz/corpus/roundtrip
cargo +nightly fuzz run -a roundtrip fuzz/corpus/roundtrip tests/data -- -max_total_time=60
```

The first directory collects the inputs libFuzzer keeps and has to exist, and
the others only seed it. The objects in SDCC's libraries make a wider seed:
extract one with `sdar x /usr/share/sdcc/lib/z80/z80.lib` in an empty directory
and pass that directory as well. An input that fails is saved under
`fuzz/artifacts/roundtrip/` and can be rerun by passing it alone.
