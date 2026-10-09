use std::error::Error;
use std::path::Path;
use std::process::ExitCode;

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let (input, output) = match args.as_slice() {
        [input] => (input, None),
        [input, output] => (input, Some(output.as_str())),
        _ => {
            eprintln!("Usage: rel2elf <input.rel|input.lib> [output.o|output.a]");
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
    let (converted, extension) = if data.starts_with(b"!<arch>\n") {
        (sdcconv::lib_to_elf_archive(&data)?, "a")
    } else {
        let text = std::str::from_utf8(&data)
            .map_err(|_| format!("'{input}' is neither a .rel object nor an archive"))?;
        (sdcconv::rel_to_elf(&asxxxx::parse(text)?)?, "o")
    };
    let output = output.map_or_else(|| Path::new(input).with_extension(extension), Into::into);
    std::fs::write(&output, converted)
        .map_err(|e| format!("cannot write '{}': {e}", output.display()))?;
    Ok(())
}
