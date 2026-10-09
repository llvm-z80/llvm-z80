use std::path::PathBuf;

use anyhow::Context;
use clap::Parser;
use object::FileKind;

/// Convert an SDCC object, or a library of them, to a Z80 ELF object or
/// archive.
#[derive(Parser)]
#[command(version)]
struct Args {
    /// Rename memcpy, memmove, memset, strlen and strcmp to <name>__sdcc, so
    /// that code built with SDCC keeps calling its own versions when linked
    /// alongside the LLVM-Z80 runtime.
    #[arg(long)]
    rename_sdcc_libc: bool,
    /// An SDCC object (.rel) or a library of them (.lib).
    input: PathBuf,
    /// Where to write, by default the input's name ending in .o or .a.
    output: Option<PathBuf>,
}

fn main() -> anyhow::Result<()> {
    let args = Args::parse();
    let data = std::fs::read(&args.input)
        .with_context(|| format!("cannot read '{}'", args.input.display()))?;
    let (converted, warnings, extension) = match FileKind::parse(&*data) {
        Ok(FileKind::Archive) => {
            let (archive, warnings) = sdcconv::lib_to_elf_archive(&data, args.rename_sdcc_libc)?;
            (archive, warnings, "a")
        }
        _ => {
            let text = std::str::from_utf8(&data).with_context(|| {
                format!(
                    "'{}' is neither an SDCC object nor a library",
                    args.input.display()
                )
            })?;
            let mut obj = asxxxx::parse(text)?;
            if args.rename_sdcc_libc {
                sdcconv::rename_libc_symbols(&mut obj);
            }
            let (elf, warnings) = sdcconv::rel_to_elf(&obj)?;
            (elf, warnings, "o")
        }
    };
    for warning in warnings {
        eprintln!("warning: {warning}");
    }
    let output = args
        .output
        .unwrap_or_else(|| args.input.with_extension(extension));
    std::fs::write(&output, converted)
        .with_context(|| format!("cannot write '{}'", output.display()))?;
    Ok(())
}
