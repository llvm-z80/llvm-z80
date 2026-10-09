use std::error::Error;
use std::path::Path;
use std::process::ExitCode;

const USAGE: &str = "Usage: rel2elf [--rename-sdcc-libc] <input.rel|input.lib> [output.o|output.a]

  --rename-sdcc-libc  Rename the C library functions SDCC's libraries share with
                      the LLVM-Z80 runtime (memcpy, memmove, memset, strlen,
                      strcmp) to <name>__sdcc, so SDCC code keeps its own.";

fn main() -> ExitCode {
    let mut rename_libc = false;
    let mut paths = Vec::new();
    for arg in std::env::args().skip(1) {
        match arg.as_str() {
            "--rename-sdcc-libc" => rename_libc = true,
            _ if arg.starts_with('-') => {
                eprintln!("{USAGE}");
                return ExitCode::FAILURE;
            }
            _ => paths.push(arg),
        }
    }
    let (input, output) = match paths.as_slice() {
        [input] => (input, None),
        [input, output] => (input, Some(output.as_str())),
        _ => {
            eprintln!("{USAGE}");
            return ExitCode::FAILURE;
        }
    };
    match convert(input, output, rename_libc) {
        Ok(()) => ExitCode::SUCCESS,
        Err(e) => {
            eprintln!("error: {e}");
            ExitCode::FAILURE
        }
    }
}

fn convert(input: &str, output: Option<&str>, rename_libc: bool) -> Result<(), Box<dyn Error>> {
    let data = std::fs::read(input).map_err(|e| format!("cannot read '{input}': {e}"))?;
    let (converted, extension) = if data.starts_with(b"!<arch>\n") {
        let (archive, warnings) = sdcconv::lib_to_elf_archive(&data, rename_libc)?;
        for warning in warnings {
            eprintln!("warning: {warning}");
        }
        (archive, "a")
    } else {
        let text = std::str::from_utf8(&data)
            .map_err(|_| format!("'{input}' is neither a .rel object nor an archive"))?;
        let mut obj = asxxxx::parse(text)?;
        if rename_libc {
            sdcconv::rename_libc_symbols(&mut obj);
        }
        (sdcconv::rel_to_elf(&obj)?, "o")
    };
    let output = output.map_or_else(|| Path::new(input).with_extension(extension), Into::into);
    std::fs::write(&output, converted)
        .map_err(|e| format!("cannot write '{}': {e}", output.display()))?;
    Ok(())
}
