# rel2elf

Converts SDCC `.rel` (object) and `.lib` (ar archive) files to Z80/SM83 ELF
format for use with `ld.lld`.

## Usage

```
rel2elf <input.rel|input.lib> [output.o|output.a]
```

## Symbol Renaming

SDCC libraries may define common C standard library symbols (`memset`,
`memcpy`, etc.) that conflict with the LLVM-Z80 runtime or other toolchain
libraries using a different calling convention.

SDCC compiles these functions using `sdcccall(0)` (caller cleanup), while
LLVM-Z80's runtime uses `sdcccall(1)` (callee cleanup). If the linker picks
the SDCC version, the calling convention mismatch corrupts the stack.

**Solution:** rel2elf automatically renames the following SDCC symbols by
appending `__sdcc`:

| Original (SDCC) | Renamed (ELF) |
|------------------|---------------|
| `_memcpy` | `_memcpy__sdcc` |
| `_memmove` | `_memmove__sdcc` |
| `_memset` | `_memset__sdcc` |
| `_strlen` | `_strlen__sdcc` |
| `_strcmp` | `_strcmp__sdcc` |

Both definitions and references are renamed, so SDCC code internally links
to its own `sdcccall(0)` version, while the rest of the toolchain links to
the `sdcccall(1)` version provided by the runtime.

### Adding new conflicting symbols

Add the symbol name to the `CONFLICTING_SYMBOLS` array in `src/main.rs`.
