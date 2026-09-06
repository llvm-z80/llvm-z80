# SM83 runtime

Compiler runtime for the `sm83` target (the Game Boy CPU): the helpers clang
emits calls to (`__mulhi3`, `__addsf3`, …), the freestanding string/memory
functions, the C startup code, and the default linker script.

It mirrors `../z80/` file for file, but the implementations are separate: SM83
has a different calling convention and is missing instructions the Z80 versions
rely on.

## Layout

| | |
|---|---|
| `<name>.asm` | one function family per file, named after its entry point |
| `crt0.asm` | startup for the ELF toolchain (`ld.lld` + `sm83.ld`) |
| `crt0_sdcc.asm` | startup for the SDCC toolchain (`sdldgb`) |
| `sm83.ld` | default linker script, flat 64 KB image from 0x0000 |
| `LICENSE` | Zlib OR Apache-2.0 WITH LLVM-exception OR MIT |

`llvm/lib/Target/Z80/CMakeLists.txt` builds `lib/sm83/sm83_rt.a` with
`llvm-mc` + `llvm-ar`, and `lib/sm83/sm83_rt.lib` with `sdasgb` + `sdar` when
those tools are installed.

## Calling convention

SDCC's `__sdcccall(1)` for SM83, which is **not** the Z80 one.

| | |
|---|---|
| 1st argument | `i8` → `A`, `i16` → `DE`, `i32`/`f32` → `DEBC` (`DE` high, `BC` low) |
| 2nd argument | `i8` after `A` → `E`, `i16` after `DE` → `BC`; larger goes on the stack |
| 3rd argument onwards | stack |
| Return | `i8` → `A`, `i16` → `BC`, `i32`/`f32` → `DEBC` |
| Callee-saved | none |

`HL` never carries an argument or a return value: it is reserved for memory
access, since SM83 addresses memory almost exclusively through it.

Values larger than 32 bits are returned through an sret pointer that the caller
pushes along with the arguments.

## What SM83 does not have

No `IX`/`IY`, no shadow registers, no `DJNZ`, no `SBC HL,rr` / `ADC HL,rr`, no
`EX DE,HL`, and no `IN`/`OUT`. There is no frame pointer at all, so stack
arguments and locals are reached with `LDHL SP,n` instead of `IX+d`, and dynamic
stack allocation (`alloca`, VLAs) is rejected by the backend.

Instructions the Z80 versions have no equivalent for:

| | |
|---|---|
| `LDI A,(HL)` = `LD A,(HL+)` | load through `HL`, then `HL++` |
| `LDD A,(HL)` = `LD A,(HL-)` | load through `HL`, then `HL--` |
| `LDI (HL),A` = `LD (HL+),A` | store through `HL`, then `HL++` |
| `LDHL SP,n` = `LD HL,SP+n` | `HL = SP + signed 8-bit offset` |

The string and memory functions lean on the auto-incrementing forms; the 32- and
64-bit division helpers lean on `LDHL SP,n` for their stack frames.

### f32 representation

IEEE 754 single precision, held in `DEBC` with `DE` as the high half. The bit
layout is the same as on Z80; only the register pair differs.

### 32-bit integers

`__divsi3` and friends use restoring division. Without shadow registers the
remainder lives on the stack rather than in `HL'`. First `i32` argument in
`DEBC`, second on the stack, result in `DEBC`.

### 64-bit integers

`__divdi3` and friends are memory-based. The caller pushes an sret pointer
(2 B), the dividend (8 B) and the divisor (8 B); the callee reserves its own
scratch with `add sp,#-n` and addresses everything through `LDHL SP,n`. Each
file documents its exact frame.

## Symbol names

The assembler prefixes C identifiers with an underscore, so:

| asm symbol | C name | what it is |
|---|---|---|
| `_memcpy` | `memcpy` | library function, callable from C |
| `___mulhi3` | `__mulhi3` | compiler runtime helper; clang emits calls to it |
| `__call_hl` | `_call_hl` | indirect-call thunk the backend emits (`JP (HL)`) |
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
3. Add the Z80 counterpart in `../z80/` if it applies.
4. `ninja Z80Runtime` builds it; `z80-utils` exercises it.
