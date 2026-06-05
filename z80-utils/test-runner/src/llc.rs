use std::path::PathBuf;
use std::process::Command;

use crate::config::{OptLevel, Paths, Target};
use crate::emulator;
use crate::suite::*;

const COMPILE_TIMEOUT: u64 = 30;

pub struct LlcConfig {
    pub target: Target,
    pub opt_levels: Vec<OptLevel>,
    pub pattern: Option<String>,
}

pub fn run(paths: &Paths, config: &LlcConfig, on_result: &mut OnResult) -> SuiteResult {
    let test_dir = paths.llc_test_dir();
    let llc = paths.llc();
    let mut result = SuiteResult::default();
    let reg_name = config.target.reg_name();

    let rt_lib = paths.rt_lib(config.target);
    if !rt_lib.exists() {
        eprintln!(
            "ERROR: runtime library not found: {}",
            rt_lib.display()
        );
        eprintln!("Run 'ninja lib/Target/Z80/Z80Runtime' to build it.");
        return result;
    }

    let tests = discover_tests(&test_dir, "test_", "ll");

    for test_file in &tests {
        let name = test_file
            .file_stem()
            .unwrap()
            .to_string_lossy()
            .to_string();

        if let Some(ref pat) = config.pattern {
            if !name.contains(pat.as_str()) {
                continue;
            }
        }

        let source = std::fs::read_to_string(test_file).unwrap_or_default();

        for &opt in &config.opt_levels {
            let tag = format!("{name}_{opt}");

            // Check SKIP-IF
            if let Some(reason) = check_skip_ll(&source, config.target, opt) {
                result.add(TestResult::skip(&tag, reason), on_result, reg_name);
                continue;
            }

            let r = run_single(
                &llc,
                test_file,
                &tag,
                config.target,
                opt,
                &test_dir,
                &rt_lib,
                &source,
            );
            result.add(r, on_result, reg_name);
        }
    }

    result
}

fn run_single(
    llc: &PathBuf,
    test_file: &PathBuf,
    tag: &str,
    target: Target,
    opt: OptLevel,
    work_dir: &PathBuf,
    rt_lib: &PathBuf,
    source: &str,
) -> TestResult {
    let tmp_dir = unique_tmp_dir(work_dir);
    let _ = std::fs::create_dir_all(&tmp_dir);

    let asm_out = tmp_dir.join(format!("{tag}.s"));
    let rel_out = tmp_dir.join(format!("{tag}.rel"));
    let out_base = tmp_dir.join(tag);
    let ihx = tmp_dir.join(format!("{tag}.ihx"));
    let bin = tmp_dir.join(format!("{tag}.bin"));

    // LLC: .ll → .s
    {
        let mut cmd = Command::new(llc.as_os_str());
        cmd.arg(format!("-mtriple={}", target.triple()));
        cmd.arg(opt.flag());
        cmd.args(["-z80-asm-format=sdasz80"]);
        cmd.arg(test_file.as_os_str());
        cmd.arg("-o");
        cmd.arg(&asm_out);

        match run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
            Err(e) => {
                remove_tmp_dir(&tmp_dir);
                return TestResult::fatal(tag, format!("llc {e}"));
            }
            Ok((code, _, stderr)) if code != 0 => {
                let err = extract_error(&stderr);
                remove_tmp_dir(&tmp_dir);
                return TestResult::fatal(tag, err);
            }
            _ => {}
        }
    }

    // Assemble: .s → .rel
    {
        let status = Command::new(target.assembler())
            .args(["-g", "-o"])
            .arg(&rel_out)
            .arg(&asm_out)
            .stdout(std::process::Stdio::null())
            .stderr(std::process::Stdio::null())
            .status();
        if !status.is_ok_and(|s| s.success()) {
            remove_tmp_dir(&tmp_dir);
            return TestResult::fatal(tag, "assembler error");
        }
    }

    // Link: .rel → .ihx
    {
        let rt_dir = rt_lib.parent().unwrap();
        let rt_name = rt_lib.file_stem().unwrap();

        let mut cmd = Command::new(target.linker());
        cmd.args(["-m", "-i"]);
        cmd.arg(&out_base);
        cmd.arg(&rel_out);
        cmd.arg("-k");
        cmd.arg(rt_dir);
        cmd.arg("-l");
        cmd.arg(rt_name);
        cmd.stdout(std::process::Stdio::null());
        cmd.stderr(std::process::Stdio::null());
        let link_status = cmd.status();
        if !link_status.is_ok_and(|s| s.success()) || !ihx.exists() {
            remove_tmp_dir(&tmp_dir);
            return TestResult::fatal(tag, "link error");
        }
    }

    // makebin
    if let Err(e) = emulator::makebin(&ihx, &bin) {
        remove_tmp_dir(&tmp_dir);
        return TestResult::fatal(tag, e);
    }

    // Emulate
    let map_file = ihx.with_extension("map");
    let halt_addr = match emulator::halt_addr_from_map(&map_file) {
        Some(addr) => addr,
        None => {
            remove_tmp_dir(&tmp_dir);
            return TestResult::fatal(tag, "_halt symbol not found in map file");
        }
    };
    let result = match emulator::emulate(&bin, target, &halt_addr) {
        Err(e) => TestResult::fatal(tag, e),
        Ok(got) => {
            let expected = emulator::parse_expected(source);
            match emulator::check_result(&got, &expected) {
                Ok(()) => TestResult::pass(tag, format!("0x{got}")),
                Err((got_padded, exp_padded)) => {
                    TestResult::fail(tag, format!("0x{got_padded}"), format!("0x{exp_padded}"))
                }
            }
        }
    };
    remove_tmp_dir(&tmp_dir);
    result
}

