use std::path::{Path, PathBuf};
use std::process::Command;

use crate::config::{OptLevel, Paths, Target};
use crate::suite::*;

const COMPILE_TIMEOUT: u64 = 120;

pub struct CustomConfig {
    pub target: Target,
    pub opt: OptLevel,
    pub files: Vec<String>,
}

/// Discover all .c and .ll files in a directory.
pub fn discover_files(dir: &Path) -> Vec<PathBuf> {
    let entries = match std::fs::read_dir(dir) {
        Ok(e) => e,
        Err(_) => return Vec::new(),
    };
    let mut files: Vec<PathBuf> = entries
        .flatten()
        .filter(|e| {
            let name = e.file_name();
            let name = name.to_string_lossy();
            (name.ends_with(".c") || name.ends_with(".ll")) && e.path().is_file()
        })
        .map(|e| e.path())
        .collect();
    files.sort();
    files
}

pub fn run(paths: &Paths, config: &CustomConfig, on_result: &mut OnResult) -> SuiteResult {
    let mut result = SuiteResult::default();
    let reg_name = config.target.reg_name();

    for file in &config.files {
        let path = Path::new(file);
        if !path.exists() {
            let tag = path.file_name().unwrap_or_default().to_string_lossy().to_string();
            result.add(
                TestResult::fatal(&tag, format!("file not found: {file}")),
                on_result,
                reg_name,
            );
            continue;
        }

        let ext = path.extension().and_then(|e| e.to_str()).unwrap_or("");
        let r = match ext {
            "ll" => compile_ir(paths, path, config),
            "c" => compile_c(paths, path, config),
            other => {
                let tag = path.file_name().unwrap().to_string_lossy().to_string();
                TestResult::fatal(&tag, format!("unsupported extension: .{other} (use .c or .ll)"))
            }
        };
        result.add(r, on_result, reg_name);
    }

    result
}

fn compile_ir(paths: &Paths, file: &Path, config: &CustomConfig) -> TestResult {
    let tag = file.file_name().unwrap().to_string_lossy().to_string();
    let llc = paths.llc();

    let mut cmd = Command::new(llc.as_os_str());
    cmd.arg(format!("-mtriple={}", config.target.triple()));
    cmd.arg(config.opt.flag());
    cmd.args(["-z80-asm-format=sdasz80"]);
    cmd.arg(file.as_os_str());
    cmd.arg("-o");
    cmd.arg("/dev/null");

    match run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
        Err(e) => TestResult::fatal(&tag, format!("llc {e}")),
        Ok((code, _, stderr)) if code != 0 => {
            TestResult::fatal(&tag, extract_error(&stderr))
        }
        Ok(_) => TestResult::pass(&tag, "compiled"),
    }
}

fn compile_c(paths: &Paths, file: &Path, config: &CustomConfig) -> TestResult {
    let tag = file.file_name().unwrap().to_string_lossy().to_string();
    let clang = paths.clang();

    let mut cmd = Command::new(clang.as_os_str());
    cmd.arg(format!("--target={}", config.target.triple()));
    cmd.arg(config.opt.flag());
    cmd.args(["-S", "-o", "/dev/null"]);
    cmd.arg(file.as_os_str());

    match run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
        Err(e) => TestResult::fatal(&tag, format!("clang {e}")),
        Ok((code, _, stderr)) if code != 0 => {
            TestResult::fatal(&tag, extract_error(&stderr))
        }
        Ok(_) => TestResult::pass(&tag, "compiled"),
    }
}
