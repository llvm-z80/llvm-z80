# z80-utils

Utilities and test infrastructure for LLVM-Z80. This workspace contains three crates:

* **test-runner** — Dynamic test runner (compiles, emulates, verifies)
* **elf2rel** — ELF → SDCC .rel object format converter
* **rel2elf** — SDCC .rel → ELF object format converter

## Prerequisites
* Rust toolchain
* Built LLVM-Z80 (`ninja -C build` from the repository root)
* SDCC toolchain (`sdcc`, `sdasz80`, `sdldz80`, `sdasgb`, `sdldgb`, `makebin`)
* z88dk Z80 emulator (`z88dk-ticks`)

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
```

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

## Test Runner

Dynamic test runner for LLVM-Z80. Compiles C and LLVM IR test programs, runs them on a Z80/SM83 emulator, and verifies results.

### Quick Start

```bash
cd z80-utils
cargo run                        # Run all test suites (default: O1, O2, Os)
cargo run -- -full                  # Run all optimization levels (O0-Oz)
```

### Test Suites

#### clang — C Compilation Tests
Compiles C source files with Clang using the ELF toolchain (`--target=z80` or `--target=sm83`),
links with `ld.lld`, converts to binary via `llvm-objcopy`, and runs on the emulator.

```bash
cargo run clang                           # Z80, all opt levels
cargo run clang -target sm83 -opt O1      # SM83, O1 only
cargo run clang -ffast-math               # With -ffast-math flag
cargo run clang -omit-frame-pointer       # With -fomit-frame-pointer
```

#### sdcc — SDCC Cross-Build Compatibility Tests
Tests interoperability between Clang-compiled and SDCC-compiled code.
Compiles test pairs (`test_*_clang.c` + `test_*_sdcc.c`), links them together via
sdldz80, and verifies correct ABI interop. Uses `--target=z80-unknown-none-sdcc`
for the Clang side.

```bash
cargo run sdcc                            # Z80, all opt levels
cargo run sdcc -target sm83 -opt O1       # SM83, O1 only
```

#### llc — LLVM IR Tests
Compiles LLVM IR files with `llc`, assembles with sdasz80, and links with sdldz80.

```bash
cargo run llc                             # Z80, all opt levels
cargo run llc -target sm83 -opt O0        # SM83, O0 only
```

#### utils — elf2rel/rel2elf Converter Tests
Tests the ELF ↔ SDCC .rel format converters through roundtrip and cross-link scenarios.
Test groups run in parallel: shipped crt0, ELF roundtrip, REL roundtrip,
elf2rel crosslink, rel2elf crosslink, ELF archive roundtrip, REL archive
roundtrip. The first links the way a user does, letting the clang driver pick
the startup code, so the crt0 that actually ships stays covered.

```bash
cargo run utils                           # Z80, O1
cargo run utils -target sm83              # SM83
```

#### torture — GCC C Torture Suite
Runs `gcc.c-torture` from the `vendor/gcc-torture` submodule. Not part of the
default run: it stays red while any backend bug is outstanding, which is the
point. Nothing else needs the submodule, so a plain clone does not carry it:

```bash
git submodule update --init vendor/gcc-torture
```

```bash
cargo run torture                          # both tiers, Z80, O1
cargo run torture -tier execute -target sm83
cargo run torture -emu-timeout 60          # widen the budget for a slow test
cargo run torture -run-skipped             # re-check what the manifest skips
```

Two tiers. `compile` only asserts that clang accepts the input, which is where
compiler crashes surface most cheaply. `execute` runs self-checking tests, so
the expected result is always zero and no `expect` directive is needed.

Outcomes are `PASS`, `XFAIL` (a `dg-error` test rejected as upstream expects),
`SKIP`, `ICE`, `CLANG` (a failure the manifest attributes to clang, still run so
it turns green when clang fixes it), `FAIL`, `OPTIM` (an optimization that
should have deleted a call did not), `TIMEOUT`, `COMPILE`, `LINK`, `TOOBIG`.

`TIMEOUT` covers two different failures and can hide a miscompile. One is a
test that is merely slower than the emulation budget, which `-emu-timeout`
settles. The other is a program that reached `__builtin_trap()`: that lowers to
`HALT`, which stops the CPU somewhere other than `_halt`, so the run burns its
whole budget and looks identical to a slow test. A test that traps got a wrong
answer. z88dk-ticks does not report the final PC, so the runner cannot tell the
two apart; treat a newly appearing `TIMEOUT` as something to investigate rather
than as a slow test.

`test-runner/torture/manifest.txt` lists only what the target structurally
cannot do. A backend bug never belongs there: it keeps failing until it is
fixed. Upstream's own `dg-skip-if`, `dg-require-effective-target` and
`dg-options` are read straight from the test sources instead.

#### custom — Ad-hoc Compile Check
Checks that files in `test-runner/testcases/custom/` compile without errors (no emulation).

```bash
cargo run custom                          # Auto-discover .c/.ll files
cargo run custom file.c                   # Specific file
```

#### bench — Code Size Benchmarks
Measures compiled code size across benchmarks.

```bash
cargo run bench                           # Z80, O1
cargo run bench -target sm83 -opt Os      # SM83, Os
```

### Test File Format

#### C tests (`test-runner/testcases/clang/`, `test-runner/testcases/sdcc/`)
```c
/* Test description */
/* SKIP-IF: sm83 */                    /* Optional: skip on specific targets */
/* SKIP-IF: -ffast-math */             /* Optional: skip with specific flags */
/* SKIP-IF: -fno-integrated-as */      /* Optional: skip on external assembler path */
/* expect: 0x00FF */
int main(void) {
    uint16_t status = 0;
    if (test_passes) status |= (1 << 0);  /* Each bit = one sub-test */
    // ...
    return status;  /* Returned in DE (Z80) or BC (SM83) */
}
```

#### LLVM IR tests (`test-runner/testcases/llc/`)
```llvm
; SKIP-IF: O0 sm83
define i16 @main() {
  ; ...
  ret i16 %status
}
; expect 0x000F
```

### Targets and Triples

| Target | ELF Triple | SDCC Triple | Emulator Flag |
|--------|-----------|-------------|---------------|
| Z80    | `z80`     | `z80-unknown-none-sdcc` | (none) |
| SM83   | `sm83`    | `sm83-nintendo-none-sdcc` | `-mgbz80` |

### Environment Variables

* `BUILD_DIR` — Override the LLVM build directory (default: `../../build` relative to test-runner)

### Testcase Directories

* [`test-runner/testcases/clang/`](test-runner/testcases/clang/) — C source tests for Clang
* [`test-runner/testcases/llc/`](test-runner/testcases/llc/) — LLVM IR tests for LLC
* [`test-runner/testcases/sdcc/`](test-runner/testcases/sdcc/) — SDCC cross-build compatibility test pairs
* [`test-runner/testcases/custom/`](test-runner/testcases/custom/) — User-supplied files for compile checking
* [`vendor/gcc-torture/`](vendor/gcc-torture/) — GCC C torture suite (submodule)

### Harness Runtime

`test-runner/harness/{z80,sm83}/` holds the startup code the suites link, kept
separate from the crt0 in `compiler-rt` because it records `main`'s return value
at a symbol named `_exitcode`. The runner reads the result from there out of a
RAM dump (`z88dk-ticks -output`) rather than from a register: registers are only
visible under `-trace`, which prints every executed instruction and slows
emulation by more than two orders of magnitude.

`test-runner/torture/shim/` adds `abort`, `exit` and `link_error` for the
torture tests, which are self-checking and call `abort()` on failure.
