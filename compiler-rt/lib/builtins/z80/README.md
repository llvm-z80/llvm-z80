# Z80 runtime

Compiler runtime for the `z80` target: the helpers clang emits calls to
(`__mulhi3`, `__addsf3`, …), the freestanding string/memory functions, the C
startup code, and the default linker script.

## Layout

| | |
|---|---|
| `<name>.asm` | one function family per file, named after its entry point |
| `crt0.asm` | startup for the ELF toolchain (`ld.lld` + `z80.ld`) |
| `crt0_sdcc.asm` | startup for the SDCC toolchain (`sdldz80`) |
| `z80.ld` | default linker script, flat 64 KB image from 0x0000 |
| `LICENSE` | Zlib OR Apache-2.0 WITH LLVM-exception OR MIT |

`llvm/lib/Target/Z80/CMakeLists.txt` builds two flavours from the same sources:
`llvm-mc` + `llvm-ar` produce `lib/z80/z80_rt.a` for the ELF path, and
`sdasz80` + `sdar` produce `lib/z80/z80_rt.lib` for the SDCC path (only when
those tools are installed).

## Calling convention

Everything here follows SDCC's `__sdcccall(1)`.

| | |
|---|---|
| 1st argument | `i8` → `A`, `i16` → `HL`, `i32`/`f32` → `HLDE` |
| 2nd argument | `i16` → `DE`; anything larger goes on the stack |
| 3rd argument onwards | stack, pushed right to left, caller cleans up |
| Return | `i8` → `A`, `i16` → `DE`, `i32`/`f32` → `HLDE` |
| Callee-saved | `IX` only. `A`, `BC`, `DE`, `HL` are caller-saved |

Values larger than 32 bits are returned through an sret pointer: the caller
pushes a destination pointer along with the arguments, and the callee writes the
result there. `divmoddi3.asm` and `divmodti3.asm` document their exact frames.

### f32 representation

IEEE 754 single precision, held in `HLDE`:

```
H = SEEE EEEE   S = sign, E = exponent[7:1]
L = EMMM MMMM   E = exponent[0], M = mantissa[22:16]
D = MMMM MMMM   M = mantissa[15:8]
E = MMMM MMMM   M = mantissa[7:0]
```

Exponent bias 127; 0 means zero or denormal, 255 means infinity or NaN.
Normalized values carry an implicit leading 1, so the significand is 24 bits.

### 32-bit integers

`__divsi3` and friends use restoring division, tracking the remainder in the
shadow registers (`exx`). First `i32` argument in `HLDE`, second on the stack at
`IX+4..IX+7` once the frame is set up, result in `HLDE`.

### 64-bit integers

`__divdi3` and friends are memory-based; the registers are too small to hold the
operands. The caller pushes an sret pointer (2 B), the dividend (8 B) and the
divisor (8 B), with the sret pointer at the lowest address. Each file documents
the resulting `IX`-relative frame.

## Symbol names

The assembler prefixes C identifiers with an underscore, so:

| asm symbol | C name | what it is |
|---|---|---|
| `_memcpy` | `memcpy` | library function, callable from C |
| `___mulhi3` | `__mulhi3` | compiler runtime helper; clang emits calls to it |
| `__add_a_norm` | — | internal jump target, never referenced from outside |

**Only export what something outside the file can reach.** A `.globl` on an
internal label is not free: a global symbol may be preempted at link time, so
the assembler cannot use a relative branch to it and widens every `jr` to a
3-byte `jp`. Over-exporting internal labels this way cost 549 bytes across the
two runtimes before it was cleaned up, and it lets an unrelated undefined symbol
drag a whole archive member in.

So a symbol gets `.globl` only if it is one of:

* an entry point named in the file's doc block,
* a name the backend emits (see `llvm/lib/Target/Z80/`),
* referenced by another `.asm` here,
* an import the file needs (`crt0.asm` declaring `_main`).

## Adding a function

1. Create `<name>.asm` named after the entry point, with the SPDX line first,
   then `.area _CODE`, then the `.globl` lines.
2. Document each entry point in a `;===`-delimited block giving `Input:`,
   `Output:`, and anything surprising about the algorithm. State register usage
   in terms of the convention above rather than repeating it.
3. Add the same function to `../sm83/` if it applies; the two are separate
   implementations because the calling conventions differ.
4. `ninja Z80Runtime` builds it; `z80-utils` exercises it.
