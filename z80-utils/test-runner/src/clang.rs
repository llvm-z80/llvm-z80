use std::path::PathBuf;
use std::process::Command;

use crate::config::{OptLevel, Paths, Target};
use crate::emulator;
use crate::suite::*;

const COMPILE_TIMEOUT: u64 = 30;

pub struct ClangConfig {
    pub target: Target,
    pub opt_levels: Vec<OptLevel>,
    pub fast_math: bool,
    pub omit_fp: bool,
    pub inline_runtime: bool,
    pub pattern: Option<String>,
}

impl ClangConfig {
    fn extra_flags(&self) -> Vec<&str> {
        let mut flags = Vec::new();
        if self.fast_math {
            flags.push("-ffast-math");
        }
        if self.omit_fp {
            flags.push("-fomit-frame-pointer");
        }
        if self.inline_runtime {
            flags.push("-Xclang");
            flags.push("-target-feature");
            flags.push("-Xclang");
            flags.push("+inline-i16-runtime");
        }
        flags
    }

    fn active_options(&self) -> Vec<&str> {
        self.extra_flags()
    }

    fn tag_suffix(&self) -> String {
        let mut s = String::new();
        if self.fast_math {
            s.push_str("_ffast");
        }
        if self.omit_fp {
            s.push_str("_nofp");
        }
        if self.inline_runtime {
            s.push_str("_inlrt");
        }
        s
    }
}

pub fn run(paths: &Paths, config: &ClangConfig, on_result: &mut OnResult) -> SuiteResult {
    let test_dir = paths.clang_test_dir();
    let clang = paths.clang();
    let mut result = SuiteResult::default();
    let reg_name = config.target.reg_name();

    let tests = discover_tests(&test_dir, "test_", "c");
    let suffix = config.tag_suffix();
    let active = config.active_options();

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

        // Parse per-test EXTRA-FLAGS from source comments.
        let per_test_flags = parse_extra_flags_c(&source);

        for &opt in &config.opt_levels {
            let tag = format!("{name}_{opt}{suffix}");

            // Check SKIP-IF
            if let Some(reason) = check_skip_c(&source, config.target, &active) {
                result.add(TestResult::skip(&tag, reason), on_result, reg_name);
                continue;
            }

            let mut flags = config.extra_flags();
            for f in &per_test_flags {
                flags.push(f.as_str());
            }

            let r = run_single(
                &clang,
                test_file,
                &tag,
                config.target,
                opt,
                &flags,
                &test_dir,
                &source,
            );
            result.add(r, on_result, reg_name);
        }
    }

    result
}

fn run_single(
    clang: &PathBuf,
    test_file: &PathBuf,
    tag: &str,
    target: Target,
    opt: OptLevel,
    extra_flags: &[&str],
    work_dir: &PathBuf,
    source: &str,
) -> TestResult {
    let tmp_dir = unique_tmp_dir(work_dir);
    let _ = std::fs::create_dir_all(&tmp_dir);

    let elf = tmp_dir.join(format!("{tag}.elf"));
    let bin = tmp_dir.join(format!("{tag}.bin"));

    // Compile + link (integrated assembler + lld → ELF)
    let mut cmd = Command::new(clang.as_os_str());
    cmd.arg(format!("--target={}", target.triple()));
    cmd.arg(format!("-{}", opt.clang_flag()));
    for flag in extra_flags {
        cmd.arg(flag);
    }
    cmd.arg(test_file.as_os_str());
    cmd.arg("-o");
    cmd.arg(elf.as_os_str());

    match run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
        Err(e) => {
            remove_tmp_dir(&tmp_dir);
            return TestResult::fatal(tag, format!("compile {e}"));
        }
        Ok((code, _, stderr)) if code != 0 => {
            let err = extract_error(&stderr);
            remove_tmp_dir(&tmp_dir);
            return TestResult::fatal(tag, err);
        }
        _ => {}
    }

    // ELF → flat binary
    let objcopy = clang.parent().unwrap().join("llvm-objcopy");
    if let Err(e) = emulator::elf_to_bin(&objcopy, &elf, &bin) {
        remove_tmp_dir(&tmp_dir);
        return TestResult::fatal(tag, e);
    }

    // Emulate
    let halt_addr = match emulator::halt_addr_from_elf(
        &clang.parent().unwrap().join("llvm-nm"), &elf) {
        Some(addr) => addr,
        None => return TestResult::fatal(tag, "_halt symbol not found in ELF"),
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

