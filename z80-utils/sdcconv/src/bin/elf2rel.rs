use std::error::Error;
use std::path::Path;
use std::process::ExitCode;

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let (input, output) = match args.as_slice() {
        [input] => (input, None),
        [input, output] => (input, Some(output.as_str())),
        _ => {
            eprintln!("Usage: elf2rel <input.o|input.a> [output.rel|output.lib]");
            return ExitCode::FAILURE;
        }
    };
    match convert(input, output) {
        Ok(()) => ExitCode::SUCCESS,
        Err(e) => {
            eprintln!("error: {e}");
            ExitCode::FAILURE
        }
    }
}

fn convert(input: &str, output: Option<&str>) -> Result<(), Box<dyn Error>> {
    let data = std::fs::read(input).map_err(|e| format!("cannot read '{input}': {e}"))?;
    let (converted, warnings, extension) = if data.starts_with(b"!<arch>\n") {
        let (lib, warnings) = sdcconv::elf_archive_to_lib(&data)?;
        (lib, warnings, "lib")
    } else {
        let module = Path::new(input).file_stem().unwrap_or_default();
        let (obj, warnings) = sdcconv::elf_to_rel(&data, &module.to_string_lossy())?;
        (asxxxx::write(&obj)?.into_bytes(), warnings, "rel")
    };
    for warning in warnings {
        eprintln!("warning: {warning}");
    }
    let output = output.map_or_else(|| Path::new(input).with_extension(extension), Into::into);
    std::fs::write(&output, converted)
        .map_err(|e| format!("cannot write '{}': {e}", output.display()))?;
    Ok(())
}
