# z80-utils

Utilities and test infrastructure for LLVM-Z80. This workspace contains three
crates:

| Crate | Description |
|-------|-------------|
| [test-runner](test-runner/) | Builds test programs with LLVM-Z80 and SDCC, runs them on an emulator and checks the results |
| [sdcconv](sdcconv/) | `elf2rel` and `rel2elf`, converters between Z80 ELF objects and SDCC `.rel` objects |
| [asxxxx](asxxxx/) | Library for reading and writing the ASxxxx (`.rel`) objects of every SDCC port, used by sdcconv |

## Prerequisites

* Rust toolchain
* Built LLVM-Z80 (`ninja -C build` from the repository root)
* SDCC toolchain (`sdcc`, `sdasz80`, `sdldz80`, `sdasgb`, `sdldgb`, `makebin`)
* z88dk Z80 emulator (`z88dk-ticks`)

## Common commands

```bash
cd z80-utils
cargo run test                     # the quick end-to-end check
cargo run full                     # everything a regression check needs
cargo test -p asxxxx -p sdcconv    # the library tests
cargo install --path sdcconv       # elf2rel and rel2elf, which the utils suite runs from PATH
```

A plain `cargo build` builds only the test runner, the workspace's default
member; name the other crates with `-p` to build them.
