use std::path::PathBuf;

use anyhow::Context;
use clap::Parser;
use object::FileKind;

/// Convert a Z80 ELF object, or an archive of them, to an SDCC object or
/// library.
#[derive(Parser)]
#[command(version)]
struct Args {
    /// An ELF object (.o) or an archive of them (.a).
    input: PathBuf,
    /// Where to write, by default the input's name ending in .rel or .lib.
    output: Option<PathBuf>,
}

fn main() -> anyhow::Result<()> {
    let args = Args::parse();
    let data = std::fs::read(&args.input)
        .with_context(|| format!("cannot read '{}'", args.input.display()))?;
    let (converted, warnings, extension) = match FileKind::parse(&*data) {
        Ok(FileKind::Archive) => {
            let (lib, warnings) = sdcconv::elf_archive_to_lib(&data)?;
            (lib, warnings, "lib")
        }
        _ => {
            let module = args.input.file_stem().unwrap_or_default();
            let (obj, warnings) = sdcconv::elf_to_rel(&data, &module.to_string_lossy())?;
            (asxxxx::write(&obj)?.into_bytes(), warnings, "rel")
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
