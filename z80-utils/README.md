# z80-utils

Utilities for LLVM-Z80. This workspace contains two crates:

* **elf2rel** — ELF → SDCC .rel object format converter
* **rel2elf** — SDCC .rel → ELF object format converter

## Prerequisites
* Rust toolchain
* Built LLVM-Z80 (`ninja -C build` from the repository root)
* SDCC toolchain (`sdcc`, `sdasz80`, `sdldz80`, `sdasgb`, `sdldgb`)

## elf2rel / rel2elf — Object Format Converters

Bidirectional converters between Z80 ELF objects and SDCC .rel (ASxxxx) objects.
These enable cross-linking between Clang-compiled (ELF) and SDCC-compiled (.rel) code.

### Supported conversions

| Tool | Input | Output |
|------|-------|--------|
| `elf2rel` | `.o` (ELF object) | `.rel` (SDCC object) |
| `elf2rel` | `.a` (ar archive of ELF) | `.lib` (ar archive of .rel) |
| `rel2elf` | `.rel` (SDCC object) | `.o` (ELF object) |
| `rel2elf` | `.lib` (ar archive of .rel) | `.a` (ar archive of ELF) |

### Usage

```bash
cargo build -p elf2rel -p rel2elf --release

# Single object conversion
elf2rel input.o output.rel
rel2elf input.rel output.o

# Archive conversion
elf2rel input.a output.lib
rel2elf input.lib output.a

# Name the CPU of modules assembled with sdas, which do not record one
rel2elf --cpu=sm83 input.lib output.a
```

rel2elf writes the CPU into the ELF header so that lld can refuse to mix
objects for different CPUs. SDCC-compiled modules record their CPU and
rel2elf uses that; for other modules it uses `--cpu` (`z80`, `z180`, `r800`,
`ez80`, `sm83` or `z80n`). A module with neither is treated as Z80.

### Cross-linking workflow

```bash
# Scenario: Link Clang-compiled code with SDCC-compiled code via sdldz80

# 1. Compile with Clang (ELF)
clang --target=z80 -c -O1 my_module.c -o my_module.o

# 2. Convert ELF → .rel
elf2rel my_module.o my_module.rel

# 3. Link with SDCC objects
sdldz80 -i output crt0.rel my_module.rel sdcc_code.rel

# Reverse: Link SDCC code into an ELF binary via lld
rel2elf sdcc_code.rel sdcc_code.o
clang --target=z80 my_module.c sdcc_code.o -o output.elf
```

