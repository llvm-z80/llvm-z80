use std::path::PathBuf;
use std::process::Command;

use crate::config::{self, OptLevel, Paths, Target};
use crate::emulator;
use crate::suite::*;

const COMPILE_TIMEOUT: u64 = 30;

pub struct SdccConfig {
    pub target: Target,
    pub opt_levels: Vec<OptLevel>,
    pub omit_fp: bool,
    pub pattern: Option<String>,
}

/// Results the suite will emit: each discovered test pair, once per
/// optimisation level.
pub fn count(paths: &Paths, config: &SdccConfig) -> u32 {
    crate::utils::discover_sdcc_test_names(&paths.sdcc_test_dir())
        .iter()
        .filter(|n| config.pattern.as_deref().is_none_or(|p| n.contains(p)))
        .count() as u32
        * config.opt_levels.len() as u32
}

pub fn run(paths: &Paths, config: &SdccConfig, on_result: &mut OnResult) -> SuiteResult {
    let test_dir = paths.sdcc_test_dir();
    let clang = paths.clang();
    let mut result = SuiteResult::default();
    let reg_name = config.target.reg_name();

    let sdcc_lib = config::find_sdcc_lib(config.target);

    // Discover test pairs: test_*_clang.c
    let test_names = crate::utils::discover_sdcc_test_names(&test_dir);

    for test_name in &test_names {
        if let Some(ref pat) = config.pattern {
            if !test_name.contains(pat.as_str()) {
                continue;
            }
        }

        for &opt in &config.opt_levels {
            let tag = format!("{test_name}_{opt}");
            let r = run_single(
                &clang,
                &test_dir,
                test_name,
                &tag,
                config.target,
                opt,
                config.omit_fp,
                paths,
                sdcc_lib.as_ref(),
            );
            result.add(r, on_result, reg_name);
        }
    }

    result
}

fn run_single(
    clang: &PathBuf,
    test_dir: &PathBuf,
    test_name: &str,
    tag: &str,
    target: Target,
    opt: OptLevel,
    config_omit_fp: bool,
    paths: &Paths,
    sdcc_lib: Option<&PathBuf>,
) -> TestResult {
    let clang_src = test_dir.join(format!("{test_name}_clang.c"));
    let sdcc_src = test_dir.join(format!("{test_name}_sdcc.c"));

    if !clang_src.exists() || !sdcc_src.exists() {
        return TestResult::fatal(tag, "missing source files");
    }

    let tmp_dir = unique_tmp_dir(test_dir);
    let _ = std::fs::create_dir_all(&tmp_dir);

    let assembler = target.assembler();
    let linker = target.linker();

    // Step 1: Compile SDCC source → .asm → .rel
    let sdcc_asm = tmp_dir.join(format!("{tag}_sdcc.asm"));
    let sdcc_rel = tmp_dir.join(format!("{tag}_sdcc.rel"));
    {
        let mut cmd = Command::new("sdcc");
        cmd.args([target.sdcc_flag(), "--std-c11", "-S"]);
        cmd.arg(&sdcc_src);
        cmd.arg("-o");
        cmd.arg(&sdcc_asm);
        match run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
            Err(e) => {
                remove_tmp_dir(&tmp_dir);
                return TestResult::fatal(tag, format!("SDCC compile: {e}"));
            }
            Ok((code, _, stderr)) if code != 0 => {
                let err = stderr.lines().next().unwrap_or("error").trim();
                remove_tmp_dir(&tmp_dir);
                return TestResult::fatal(tag, format!("SDCC compile: {err}"));
            }
            _ => {}
        }

        let status = Command::new(assembler)
            .args(["-g", "-o"])
            .arg(&sdcc_rel)
            .arg(&sdcc_asm)
            .stdout(std::process::Stdio::null())
            .stderr(std::process::Stdio::null())
            .status();
        if !status.is_ok_and(|s| s.success()) {
            remove_tmp_dir(&tmp_dir);
            return TestResult::fatal(tag, "SDCC assemble failed");
        }
    }

    // Step 2: Compile Clang source → .rel (via SDCC toolchain)
    let clang_rel = tmp_dir.join(format!("{tag}_clang.rel"));
    {
        let mut cmd = Command::new(clang.as_os_str());
        cmd.arg(format!("--target={}", target.sdcc_triple()));
        cmd.arg("-c");
        cmd.arg(format!("-{}", opt.clang_flag()));
        if config_omit_fp {
            cmd.arg("-fomit-frame-pointer");
        }
        cmd.arg(&clang_src);
        cmd.arg("-o");
        cmd.arg(&clang_rel);
        match run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
            Err(e) => {
                remove_tmp_dir(&tmp_dir);
                return TestResult::fatal(tag, format!("Clang compile: {e}"));
            }
            Ok((code, _, stderr)) if code != 0 => {
                let err = extract_error(&stderr);
                remove_tmp_dir(&tmp_dir);
                return TestResult::fatal(tag, format!("Clang compile: {err}"));
            }
            _ => {}
        }
    }

    // Step 3: Link
    let is_reverse = test_name.contains("reverse");
    let (main_rel, lib_rel) = if is_reverse {
        (&sdcc_rel, &clang_rel)
    } else {
        (&clang_rel, &sdcc_rel)
    };

    let ihx = tmp_dir.join(format!("{tag}.ihx"));
    let bin = tmp_dir.join(format!("{tag}.bin"));
    let out_base = tmp_dir.join(tag);
    {
        // The harness's own crt0: it records main's return value at
        // _exitcode so the result can be read from a RAM dump.
        let crt0 = match crate::runtime::ensure_sdcc_crt0(paths, target) {
            Ok(p) => p,
            Err(e) => {
                remove_tmp_dir(&tmp_dir);
                return TestResult::fatal(tag, format!("harness crt0: {e}"));
            }
        };
        let rt = paths.rt_lib(target);

        let mut cmd = Command::new(linker);
        cmd.args(["-m", "-i"]);
        cmd.arg(&out_base);
        cmd.arg(&crt0);
        cmd.arg(main_rel);
        cmd.arg(lib_rel);
        cmd.arg(&rt);
        if let Some(lib) = sdcc_lib {
            cmd.args(["-l"]);
            cmd.arg(lib);
        }
        cmd.stdout(std::process::Stdio::null());
        cmd.stderr(std::process::Stdio::null());
        let link_status = cmd.status();
        if !link_status.is_ok_and(|s| s.success()) || !ihx.exists() {
            remove_tmp_dir(&tmp_dir);
            return TestResult::fatal(tag, "link failed");
        }
    }

    // Step 4: makebin
    if let Err(e) = emulator::makebin(&ihx, &bin) {
        remove_tmp_dir(&tmp_dir);
        return TestResult::fatal(tag, e);
    }

    // Step 5: Emulate
    let source_file = if is_reverse { &sdcc_src } else { &clang_src };
    let source = std::fs::read_to_string(source_file).unwrap_or_default();

    let map_file = out_base.with_extension("map");
    let halt_addr = match emulator::halt_addr_from_map(&map_file) {
        Some(addr) => addr,
        None => {
            remove_tmp_dir(&tmp_dir);
            return TestResult::fatal(tag, "_halt symbol not found in map file");
        }
    };
    let result_addr = match emulator::symbol_addr_from_map(&map_file, "_exitcode") {
        Some(a) => a,
        None => {
            remove_tmp_dir(&tmp_dir);
            return TestResult::fatal(tag, "_exitcode symbol not found in map file");
        }
    };
    let dump = tmp_dir.join(format!("{tag}.ram"));
    let result = match emulator::run_program(
        &bin, target, &halt_addr, result_addr, &dump, target.emu_timeout_secs())
        .map(|r| r.value)
    {
        Err(e) => TestResult::fatal(tag, e),
        Ok(got) => {
            let expected = emulator::parse_expected(&source);
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

