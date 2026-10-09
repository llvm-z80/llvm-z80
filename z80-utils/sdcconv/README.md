# sdcconv

Converts between the Z80 ELF objects Clang writes and the ASxxxx objects
(`.rel`) the SDCC tools use, so that code built by either can be linked with
the other's linker. The library comes with two tools, `elf2rel` and `rel2elf`.
The `.rel` format itself is read and written by [asxxxx](../asxxxx/).

## Tools

| Tool | Input | Output |
|------|-------|--------|
| `elf2rel` | `.o` (ELF object) | `.rel` (SDCC object) |
| `elf2rel` | `.a` (archive of ELF objects) | `.lib` (SDCC library) |
| `rel2elf` | `.rel` (SDCC object) | `.o` (ELF object) |
| `rel2elf` | `.lib` (SDCC library) | `.a` (archive of ELF objects) |

```bash
cargo install --path sdcconv

elf2rel input.o [output.rel]
elf2rel input.a [output.lib]
rel2elf [--rename-sdcc-libc] input.rel [output.o]
rel2elf [--rename-sdcc-libc] input.lib [output.a]
```

Without an output path, the result is written next to the input with the new
extension. `--help` lists the options. Warnings, such as a weak symbol becoming
an ordinary one or an archive member that could not be converted and was left
out, go to standard error.

### Cross-linking

```bash
# Link Clang-built code with SDCC-built code using sdldz80
clang --target=z80 -c -O1 my_module.c -o my_module.o
elf2rel my_module.o my_module.rel
sdldz80 -i output crt0.rel my_module.rel sdcc_code.rel

# Link SDCC-built code into an ELF program using lld
rel2elf sdcc_code.rel sdcc_code.o
clang --target=z80 my_module.c sdcc_code.o -o output.elf
```

## Sections and areas

The areas follow the conventions of SDCC's crt0, which zeroes `_DATA` and
copies `_INITIALIZER` over `_INITIALIZED` at startup.

| ELF section | SDCC area |
|-------------|-----------|
| executable or read-only (`.text`, `.rodata`, ...) | `_CODE` |
| `.text.home`, `.text.gsinit`, `.text.gsfinal` | `_HOME`, `_GSINIT`, `_GSFINAL` |
| uninitialized (`.bss`) | `_DATA` |
| initialized and writable (`.data`) | symbols in `_INITIALIZED`, bytes in `_INITIALIZER` |
| a name starting with `_` | the area of the same name |

rel2elf maps the same areas back, writing the `_INITIALIZED` and
`_INITIALIZER` pair as one `.data` section, and keeps the name of any other
area, adding a leading `_` if it lacks one.

A section named `_DATA`, `_BSS`, `_INITIALIZED`, `_INITIALIZER`, `_CABS` or
`_DABS` is refused, because its contents would be lost or misplaced there. The
other SDCC areas, such as `_HOME`, can be named directly.

Module names come from file names, with each character a module name cannot
hold replaced by `_`.

## Relocations

The relocations of the Z80 family are converted: 16-bit words and 8-bit
values, the low and high byte of an address, PC-relative bytes and words
(`jr`, `djnz`), and the SM83 `ldh` operand. A PC-relative byte is written the
way sdas writes it, so that the linker range-checks the whole value.

Anything the other side cannot hold is refused rather than converted into
something that links differently, including:

- relocations of other SDCC ports, such as 24-bit fields or 8051 jumps
- symbol and area names a `.rel` object cannot hold
- common symbols (compile with `-fno-common`)
- relocation sections other than RELA, relocations outside their section's
  data or overlapping each other, and addends too large for their field
- big-endian objects, and objects larger than any Z80 address space

## Renaming C library functions

The LLVM-Z80 runtime defines `memcpy`, `memmove`, `memset`, `strlen` and
`strcmp`, and a library built with SDCC may define them again, not always
with the same calling convention. `rel2elf --rename-sdcc-libc` renames these
symbols to `<name>__sdcc`, definitions and references alike, so that such code
keeps calling its own versions when linked alongside the runtime. Pass it both
for the library and for any object compiled against headers that declare those
functions with the library's calling convention.

```bash
rel2elf --rename-sdcc-libc /usr/share/sdcc/lib/z80/z80.lib z80.a
```

## Tests

```bash
cargo test -p sdcconv
```

The test runner's `utils` suite covers the tools end to end; see
[test-runner](../test-runner/).

## Fuzzing

`fuzz/` holds [cargo-fuzz](https://github.com/rust-fuzz/cargo-fuzz) targets for
both directions. `rel2elf` checks that what rel2elf writes converts back and
survives a second round trip unchanged; `elf2rel` checks that what elf2rel
accepts writes as a `.rel` object that reads back the same. They need a
nightly toolchain.

```bash
cd sdcconv
mkdir -p fuzz/corpus/rel2elf fuzz/corpus/elf2rel
cargo +nightly fuzz run -a rel2elf fuzz/corpus/rel2elf ../asxxxx/tests/data -- -max_total_time=60
cargo +nightly fuzz run -a elf2rel fuzz/corpus/elf2rel <directory of ELF objects> -- -max_total_time=60
```

SDCC library objects make good seeds for `rel2elf`, and objects from
`clang --target=z80 -c` or rel2elf for `elf2rel`.
