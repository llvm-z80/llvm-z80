# z80-test

```bash
git submodule update --init --recursive --depth 1 z80-test/vendor
cmake -G Ninja -S llvm -B build -C clang/cmake/caches/Z80.cmake \
  -DLLVM_EXTERNAL_PROJECTS=z80-test \
  -DLLVM_EXTERNAL_Z80_TEST_SOURCE_DIR=$PWD/z80-test
ninja -C build check-z80
```

End-to-end tests for LLVM-Z80 and the runner they use. The tests compile
programs with this repository's Clang, link them, and run them on z80-run.
`ninja check-z80` runs them together with the Z80 tests in `llvm/`, `clang/`
and `lld/`.

## Running

`check-z80` builds the tools the tests use and runs these suites:

| Suite | Contents |
|-------|----------|
| LLVM | `llvm/test/CodeGen/Z80`, `llvm/test/MC/Z80` |
| Clang, lld | tests named `*z80*` or `*sm83*` in `clang/test/*/` and `lld/test/ELF/`, and `clang/test/Driver/mdouble.c` |
| z80-tools | `tools/test/`, the tests of the tools in `tools/` |
| z80-e2e | `e2e/`, once per configuration |
| z80-torture | `vendor/gcc-torture`, once per configuration |

SDCC is optional. With `sdasz80` and `sdldz80` (`sdasgb` and `sdldgb` for the
SM83) on `PATH`, the SDCC-format configurations run, and with `sdcc` itself the
`sdcc-abi` tests run too. Without them those tests are reported as unsupported.

One configuration or one test can be run on its own:

```bash
build/bin/llvm-lit build/tools/z80-test/e2e/z80-Os-elf
build/bin/llvm-lit build/tools/z80-test/torture/sm83-O2 --filter execute/pr
LIT_FILTER=z80-torture ninja -C build check-z80
```

The configurations of `check-z80` come from these CMake cache variables:

| Variable | Default |
|----------|---------|
| `Z80_E2E_CPUS` | `z80;sm83` |
| `Z80_E2E_OPT_LEVELS` | `O0;O1;O2;O3;Os;Oz` |
| `Z80_E2E_VARIANTS` | `base;omit-fp;inline-rt` |
| `Z80_E2E_FORMATS` | `elf;sdcc` |
| `Z80_TORTURE_CPUS` | `z80;sm83` |
| `Z80_TORTURE_OPT_LEVELS` | `O2;Os` |

`ninja -C build check-z80-all` runs the end-to-end and torture tests at every
optimization level with the variants below, all of them with
`-verify-machineinstrs`.

| Variant | Flags |
|---------|-------|
| `omit-fp` | `-fomit-frame-pointer` (Z80 only) |
| `no-static-frames` | `-mllvm -z80-static-frames=false` (`-O1` and above) |
| `inline-rt` | `-Xclang -target-feature -Xclang +inline-i16-runtime` |
| `fast-math` | `-ffast-math` (only the f32 tests in `e2e/c`) |
| `verify` | `-mllvm -verify-machineinstrs` |

## z80-run

z80-run is a Z80 code runner that uses the z88dk-ticks CPU core. The goal is to
read a linked ELF or Intel HEX file and test the program quickly.

* z80-run loads an ELF or Intel HEX file, takes the CPU from the ELF header,
  and reads the result from a register or a variable when the program halts.
* z80-run stops at `HALT` and at a jump to itself.
* Memory is a flat 64 KB array. I/O ports, interrupts and memory banking are
  not emulated, and there is no debugger.
* The emulator core is built at `-O3` with profile-guided optimization, trained
  on CoreMark compiled by this repository's Clang and by SDCC.

With these features left out on purpose and with suitable optimizations,
z80-run runs 3 to 5 times as fast as z88dk-ticks compiled the usual way.

```bash
z80-run [options] <ELF or Intel HEX file>
```

| Option | Meaning |
|--------|---------|
| `--cpu=z80\|sm83` | CPU to emulate. Defaults to the CPU in the ELF header |
| `--result=de\|bc\|hl\|<variable>` | where the result is read when the program halts. Defaults to `de` on the Z80 and `bc` on the SM83 |
| `--expect-result=<value>\|any` | value the result must have. Defaults to 0 |
| `--max-cycles=<n>` | T-states the program may spend, with an optional `k`, `M`, `G` or `T` suffix. Defaults to `10G` |
| `--report=<file>\|-` | writes the outcome as JSON |
| `-v` | describes the outcome even when the program passes |

The program runs from its entry point until one of these:

* a `HALT`, after which the result is checked
* a taken jump to itself, which would never end
* the cycle budget runs out
* an `ED FE` trap, or on the SM83 an opcode that locks up the CPU

`-DZ80_RUN_PGO=OFF` turns profile-guided optimization off. Otherwise the host
compiler builds an instrumented core, runs it on the programs in
`tools/z80-run/pgo/corpus.txt`, and rebuilds the core with the profile. The
host compiler has to be GCC 11 or later, or a Clang with a matching
`llvm-profdata`.
`tools/z80-run/pgo/build-corpus.sh <build directory>` rebuilds the programs
from the CoreMark submodule with that build's Clang and with SDCC.

## End-to-end tests

The RUN lines of a test compile the program and run it on z80-run with the
expected result:

```c
// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x00FF %t
// UNSUPPORTED: sm83
```

| Substitution | Meaning |
|--------------|---------|
| `%cc` | `clang` for the configuration's CPU, optimization level, variant and format |
| `%llc` | `llc` for the configuration's CPU and optimization level |
| `%run` | `z80-run` for the configuration's CPU |
| `%{sdcc}` | `sdcc` for the configuration's CPU |
| `%{sdcc-lib}` | SDCC's library for the configuration's CPU |

The features for `UNSUPPORTED:` and `REQUIRES:` are the CPU (`z80`, `sm83`),
the optimization level (`O0` to `Oz`), the variants, the format (`elf`, `sdcc`)
and `sdcc-compiler`.

The tests are in three directories:

* `e2e/c/`: C programs.
* `e2e/ir/`: LLVM IR programs.
* `e2e/sdcc-abi/`: programs with one half compiled by SDCC.

The two halves of an `sdcc-abi` test are in one file, split with `split-file`:

```c
// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x000F %t/prog

//--- clang.c
...
//--- sdcc.c
...
```

## GCC torture tests

A test in `vendor/gcc-torture/compile` passes when it compiles. A test in
`vendor/gcc-torture/execute` is linked with `torture/crt0/<cpu>.asm` and passes
when `main` returns 0. A call to `abort()` ends the run with the result
`0xDEAD`.

`torture/manifest.txt` lists the tests this target cannot pass, with a
`skip=<reason>` such as `libc` or `int16`, and the failures that are Clang's,
with a `clang=<note>`.

| Result | Meaning |
|--------|---------|
| Passed | the test compiled, and an execute test returned 0 |
| Failed | an execute test returned something else or hit a fault |
| Timed Out | the program did not halt |
| Crashed | the compiler crashed or the machine verifier complained |
| Rejected | Clang rejected the source |
| Link Failed | the link failed |
| Not Optimized | a call to `link_error` that should have been optimized away was left in |
| Too Large | the program does not fit in 64 KB |
| Expectedly Failed | a test with `dg-error` was rejected |
| Unexpectedly Passed | a test with `dg-error` compiled |
| Clang Bug | a failure the manifest attributes to Clang; not counted as a failure |
| Unsupported | the manifest or a `dg-` directive skips the test |

Two lit parameters change a run:

```bash
build/bin/llvm-lit --param run-skipped build/tools/z80-test/torture/z80-Os
build/bin/llvm-lit --param verify build/tools/z80-test/torture/z80-Os
```

`run-skipped` runs only the skipped tests and reports the ones that pass as
`Needlessly Skipped`. `verify` adds `-verify-machineinstrs`.

## Licenses

z80-run and the tests in this directory are distributed under the Apache
License 2.0 with LLVM Exceptions.

z88dk is under the Clarified Artistic License. A z80-run binary contains the
CPU core of z88dk.

gcc-torture is under the GNU General Public License version 3.

CoreMark is under the Apache License 2.0 with EEMBC's acceptable use agreement.
The CoreMark port and the corpus programs in `tools/z80-run/pgo` are derived
from CoreMark and are under the same license.
