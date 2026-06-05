<p align="center">
  <img width="2560" alt="LLVM-Z80" src="https://github.com/user-attachments/assets/0425ae08-9ecb-4270-840d-90145678df09">
  <br/>
  <sub>Artwork by zlfn. Created with <a href="https://www.aseprite.org/">Aseprite</a>. Lettering assisted by AI & fonts.</sub>
</p>

LLVM-Z80 is a LLVM fork supporting the Zilog Z80 series of microprocessors.  
[[Backend Code]](https://github.com/llvm-z80/llvm-z80/tree/main/llvm/lib/Target/Z80) | [[Tests / Utilities]](https://github.com/llvm-z80/llvm-z80/tree/main/z80-utils) | [[Wiki]](https://github.com/llvm-z80/llvm-z80/wiki) | [[FAQ]](https://github.com/llvm-z80/llvm-z80/wiki/FAQ) | [[Prebuilt Binaries]](https://github.com/llvm-z80/llvm-z80/releases) | [[AUTHORS]](https://github.com/llvm-z80/llvm-z80/blob/main/AUTHORS) | [[NOTICE]](https://github.com/llvm-z80/llvm-z80/blob/main/NOTICE)

## Notice

The llvm-z80 project is not officially affiliated with or endorsed by the LLVM
Foundation or LLVM project. Our project is a fork of LLVM that provides a new
backend/target; our project is based on LLVM, not a part of LLVM. Our use of
LLVM or other related trademarks does not imply affiliation or endorsement.

**Generative AI played significant role in generating code for the development of LLVM-Z80**,
which is currently is an experimental stage. Although the project aims to produce highly optimized,
production-grade code, it is not yet stable enough for use in professional production environments.

If you require a Z80 C compiler for production purposes, please use [SDCC](https://sdcc.sourceforge.net/)

## Target
**Z80, binary-compatible CPUs, and the SM83 (GBZ80).**
Z80, Z180, eZ80, Z80N, R800, μPD780, LH0080, SM83 (GBZ80), AP (Analogue Pocket). <sup><a href="#architecture-todo">†1</a></sup>

Physical Hardware Validation : TODO  
Emulation Tested : Z80, SM83

## Feature
* Integer arithmetic for types **up to 128-bit width.** <sup><a href="#integer-arithmetic">†2</a></sup>
* Half and single-precision floating-point arithmetic in **full compliance with the IEEE 754 standard.** <sup><a href="#float-arithmetic">†3</a></sup><sup><a href="#fast-math">†4</a></sup>
* SDCC `__sdcccall(1)` / `__sdcccall(0)` compatible calling convention
* Dual toolchain support:
  * **ELF path** (default): integrated assembler + `ld.lld` linker (`--target=z80`)
  * **SDCC path**: sdasz80 assembler + sdldz80 linker (`--target=z80-unknown-none-sdcc`)
* Cross-linking with SDCC-compiled code via [elf2rel / rel2elf](https://github.com/zlfn/llvm-z80/tree/main/z80-utils) converters
* GlobalISel-based code generation pipeline with register bank selection and instruction selection
* Standard LLVM optimizations including constant folding, dead code elimination, copy propagation, and global register allocation

## Frontend Integration
* **C/C++** : Supported via Clang included in this repository (C++ is experimental and untested)
* **Rust** : [llvm-z80/rust-z80](https://github.com/llvm-z80/rust-z80)
* **Others** : Potential support for other LLVM-based languages (Zig, TinyGo, EmbeddedSwift)

# Building LLVM-Z80

## Prerequisites
* CMake 3.20+
* Ninja (recommended)
* SDCC toolchain (`sdasz80`, `sdldz80`): required for the SDCC toolchain path and cross-build testing

## Clone the LLVM-Z80 repository

On Linux and MacOS:

```bash
git clone https://github.com/llvm-z80/llvm-z80.git
```

On Windows:

```bash
git clone --config core.autocrlf=false https://github.com/llvm-z80/llvm-z80.git
```

If you fail to use the --config flag as above, then verification tests will fail
on Windows.

## Build the LLVM-Z80 project

```bash
cmake -C clang/cmake/caches/Z80.cmake -G Ninja -S llvm -B build
ninja -C build # Build llc + clang + lld + Z80Runtime
```

## Usage

### ELF Toolchain (default)

Uses the integrated assembler and `ld.lld` linker. Produces ELF binaries.

```bash
# Compile C to ELF
clang --target=z80 -O1 input.c -o output.elf

# Convert to flat binary and execute
llvm-objcopy -O binary output.elf output.bin
z88dk-ticks -trace output.bin
```

### SDCC Toolchain

Uses sdasz80 assembler and sdldz80 linker. Produces Intel HEX (.ihx) files.
Useful for cross-linking with SDCC-compiled code.

For more information on SDCC integration, refer [LLVM-Z80 wiki](https://github.com/llvm-z80/llvm-z80/wiki/SDCC-Interoperability)

```bash
# Compile C to Intel HEX (via sdasz80 + sdldz80)
clang --target=z80-unknown-none-sdcc -O1 input.c -o output.ihx

# Execute with z88dk-ticks
makebin output.ihx output.bin
z88dk-ticks -trace output.bin

# Compile to .rel object file only (for linking with SDCC code)
clang --target=z80-unknown-none-sdcc -c -O1 input.c -o input.rel
```

### SM83 (Game Boy)
```bash
# ELF path
clang --target=sm83 -O1 input.c -o output.elf
llvm-objcopy -O binary output.elf output.bin
z88dk-ticks -mgbz80 -trace output.bin

# SDCC path
clang --target=sm83-nintendo-none-sdcc -O1 input.c -o output.ihx
makebin output.ihx output.bin
z88dk-ticks -mgbz80 -trace output.bin
```

### Step-by-step compilation

```bash
# LLVM IR → assembly → object → link (ELF path)
clang --target=z80 -O1 -S -emit-llvm input.c -o input.ll
llc -mtriple=z80 -O1 input.ll -o input.s
clang --target=z80 input.s -o output.elf

# LLVM IR → assembly → object → link (SDCC path)
clang --target=z80 -O1 -S -emit-llvm input.c -o input.ll
llc -mtriple=z80 -O1 -z80-asm-format=sdasz80 input.ll -o input.s
sdasz80 -g -o input.rel input.s
sdldz80 -i output.ihx input.rel build/lib/z80/z80_rt.lib
```

Runtime libraries are built at `build/lib/z80/` and `build/lib/sm83/`.

### Related Works
LLVM-Z80 stands on the shoulders of the following projects.
* [LLVM](https://llvm.org/) : The LLVM Compiler Infrastructure, upstream of this project.
* [LLVM-MOS](https://llvm-mos.org/) : Another attempt to create an LLVM backend for a classic CPU, LLVM-Z80 has adopted many optimization passes from LLVM-MOS.
* [ajokela/LLVM-Z80](https://github.com/ajokela/llvm-z80) : Experimental GlobalISel based Z80 backend, this backend was developed based on this fork.
* [LLVM-eZ80](https://github.com/jacobly0/llvm-project) / [ez80-clang](https://github.com/dinoboards/ez80-clang) : The most mature LLVM backend for the eZ80.
* [gb-llvm](https://github.com/DaveDuck321/gb-llvm) : An LLVM backend for the Nintendo Game Boy (SM83), including actual game ROM examples.
* [Rust-GB](https://github.com/zlfn/rust-gb) : Compiling Rust for the Game Boy, The reason this project was born.

**Many Other Experimental LLVM-Z80 Backends**
* [gt-retro-computing](https://github.com/gt-retro-computing/llvm-z80), [euclio](https://github.com/euclio/llvm-gbz80), [Bevinsky](https://github.com/Bevinsky/llvm-gbz80)

### Footnotes
- <sup><a id="architecture-todo" href="#architecture-todo">†1</a></sup>Planned optimizations for specific CPU cores (eZ80, Z180, Z80N, etc.) and upcoming support for derivative architectures (Rabbit 2000, TLCS-90).
- <sup><a id="integer-arithmetic" href="#integer-arithmetic">†2</a></sup>Primary support for i8, i16, i32, and i64; experimental support for i128.
- <sup><a id="float-arithmetic" href="#float-arithmetic">†3</a></sup>Primary support for f32, experimental support for f16, and planned support for f64.
- <sup><a id="fast-math" href="#fast-math">†4</a></sup>The `-ffast-math` flag allows you to disable strict IEEE 754 compliance. While this improves code efficiency, it may cause floating point operations to behave unexpectedly in certain edge cases.

---

<sub><font color="gray">
LLVM is a trademark of the LLVM Foundation.
Zilog and Z80 are registered trademarks of Littelfuse, Inc.
Use of these names is for identification and compatibility purposes only.
This project is an independent, community-driven effort and is not affiliated with, sponsored, or endorsed by the LLVM Foundation or Littelfuse, Inc.
</font></sub>
