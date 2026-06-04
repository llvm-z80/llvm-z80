use std::path::PathBuf;
use std::process::Command;

use crate::config::{OptLevel, Paths, Target};
use crate::emulator;
use crate::runtime::{self, ElfRuntime};
use crate::suite::*;

const COMPILE_TIMEOUT: u64 = 30;

pub struct ClangConfig {
    pub target: Target,
    pub opt_levels: Vec<OptLevel>,
    pub fast_math: bool,
    pub omit_fp: bool,
    pub inline_runtime: bool,
    pub static_stack: bool,
    /// Append `-mllvm -verify-machineinstrs`: run the MachineVerifier after
    /// every pass and fail the compile on invalid MIR (undefined physreg,
    /// stale liveins, bad reg classes). Catches the peephole-liveness bug
    /// family (e.g. ravn/llvm-z80#199) that otherwise stays latent because
    /// the shipped build never verifies. Works on the Release build; point
    /// BUILD_DIR at an assertions build to add the internal assert() layer.
    pub verify: bool,
    /// Cross-opt-level differential oracle: a correct program returns the SAME
    /// value at every optimization level, so any disagreement among O0..Oz for
    /// the same test is a miscompile -- independent of the hardcoded `expect`
    /// directive (which may itself be wrong/stale). Emits a `<name>_DIFFOPT`
    /// failure naming the disagreeing levels. Would have caught ravn/llvm-z80#202 (its test_54:
    /// O0_ss=0x0080 vs O1+_ss=0x00FF) with no hand-written test. Strongest with
    /// `-full` (all opt levels).
    pub diff_opt: bool,
    /// Native-reference differential oracle: compile + run each test with the
    /// host C compiler (env CC, else cc/clang/gcc) and compare its return value
    /// to the Z80 result. Unlike -diff-opt (which only finds opt-level
    /// disagreement), this catches values that are *consistently* wrong on Z80 -
    /// and unlike the `expect` directive, the reference is computed, not
    /// hand-written (so a wrong `expect` can't hide a bug). Emits a
    /// `<name>_NATIVE` failure when any opt level disagrees with the host.
    /// Caveat: host `int` is 32-bit vs Z80's 16-bit, so tests relying on 16-bit
    /// `int` wraparound can legitimately differ - triage such hits.
    pub native_oracle: bool,
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
        if self.static_stack {
            flags.push("-Xclang");
            flags.push("-target-feature");
            flags.push("-Xclang");
            flags.push("+static-stack");
        }
        if self.verify {
            flags.push("-mllvm");
            flags.push("-verify-machineinstrs");
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
        if self.static_stack {
            s.push_str("_ss");
        }
        s
    }
}

pub fn run(paths: &Paths, config: &ClangConfig, on_result: &mut OnResult) -> SuiteResult {
    let test_dir = paths.clang_test_dir();
    let clang = paths.clang();
    let mut result = SuiteResult::default();
    let reg_name = config.target.reg_name();

    // Build crt0 + compiler-rt builtin objects once for this run.  The
    // suite is unusable without them, so a failure here aborts every test
    // up front rather than silently producing "_halt symbol not found".
    let elf_rt = match runtime::ensure_elf(paths, config.target, &clang) {
        Ok(rt) => rt,
        Err(e) => {
            let tag = format!("clang_{}_runtime", config.target.triple());
            result.add(TestResult::fatal(&tag, format!("ELF runtime: {e}")), on_result, reg_name);
            return result;
        }
    };

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

        // For the cross-opt-level differential oracle: the observed value at
        // each opt level (only Pass/Fail carry a value; Fatal/Skip don't).
        let mut opt_values: Vec<(OptLevel, String)> = Vec::new();

        for &opt in &config.opt_levels {
            let tag = format!("{name}_{opt}{suffix}");

            // Check SKIP-IF
            if let Some(reason) = check_skip_c(&source, config.target, &active, opt.clang_flag()) {
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
                &elf_rt,
            );
            // Record the observed value for the differential check.
            match &r.outcome {
                TestOutcome::Pass { reg_value } => opt_values.push((opt, normalize_hex(reg_value))),
                TestOutcome::Fail { got, .. } => opt_values.push((opt, normalize_hex(got))),
                _ => {}
            }
            result.add(r, on_result, reg_name);
        }

        // Native-reference differential: compare the Z80 results to the value
        // the host C compiler computes for the same source.  Catches
        // consistently-wrong Z80 values (which -diff-opt misses) and does not
        // trust the hand-written `expect`.
        if config.native_oracle && !opt_values.is_empty()
            && !source.contains("NATIVE-SKIP")
        {
            if let Some(reference) = native_reference(test_file) {
                if opt_values.iter().any(|(_, v)| *v != reference) {
                    let detail = std::iter::once(format!("host=0x{reference}"))
                        .chain(opt_values.iter().map(|(o, v)| format!("{o}=0x{v}")))
                        .collect::<Vec<_>>()
                        .join(" ");
                    let tag = format!("{name}_NATIVE{suffix}");
                    result.add(
                        TestResult::fail(tag, detail, "Z80 matches host C compiler"),
                        on_result,
                        reg_name,
                    );
                }
            }
        }

        // Cross-opt-level differential: every opt level of the same program
        // must return the same value (optimization is semantics-preserving).
        // A disagreement is a miscompile regardless of the `expect` directive.
        if config.diff_opt && opt_values.len() >= 2 {
            let first = &opt_values[0].1;
            if opt_values.iter().any(|(_, v)| v != first) {
                let detail = opt_values
                    .iter()
                    .map(|(o, v)| format!("{o}=0x{v}"))
                    .collect::<Vec<_>>()
                    .join(" ");
                let tag = format!("{name}_DIFFOPT{suffix}");
                result.add(
                    TestResult::fail(tag, detail, "all opt levels agree"),
                    on_result,
                    reg_name,
                );
            }
        }
    }

    result
}

/// Normalize a "0x..." value string for cross-opt-level comparison: drop the
/// "0x" prefix, leading zeros, and case (the Pass path formats the raw value
/// while the Fail path zero-pads to the expected width, so the strings must be
/// canonicalized before comparing). All-zero canonicalizes to "0".
fn normalize_hex(s: &str) -> String {
    let t = s.trim().trim_start_matches("0x").trim_start_matches("0X");
    let t = t.trim_start_matches('0').to_lowercase();
    if t.is_empty() { "0".to_string() } else { t }
}

/// Compute the reference value of a test by compiling + running it with the
/// HOST C compiler (env CC, else cc/clang/gcc) and reading `main()`'s return.
/// Returns the masked-to-16-bit, normalized hex value, or None if the host
/// toolchain is unavailable or the test does not build/run on the host (e.g.
/// target-only constructs).  The test's own stdout (CHECK prints) is ignored;
/// only the harness's `RESULT=` line is parsed.
fn native_reference(test_file: &std::path::Path) -> Option<String> {
    let cc = std::env::var("CC").unwrap_or_else(|_| "cc".to_string());
    let abs = std::fs::canonicalize(test_file).ok()?;
    let dir = std::env::temp_dir().join(format!("z80_native_{}", std::process::id()));
    let _ = std::fs::create_dir_all(&dir);
    let wrapper = dir.join("wrapper.c");
    let bin = dir.join("nativeref");
    // #define main away so the test's main becomes a callable function, then a
    // real main() prints its 16-bit return behind a sentinel.
    let src = format!(
        "#include <stdio.h>\n#include <stdlib.h>\n#define main __z80_user_main\n#include \"{}\"\n#undef main\nint main(void) {{ printf(\"\\nZ80REF=%04x\\n\", (unsigned)(__z80_user_main()) & 0xFFFFu); return 0; }}\n",
        abs.display()
    );
    std::fs::write(&wrapper, &src).ok()?;
    let compiled = Command::new(&cc)
        .args(["-w", "-O0", "-o"])
        .arg(&bin)
        .arg(&wrapper)
        .output()
        .ok()?;
    if !compiled.status.success() {
        return None; // target-only test, or host compiler missing
    }
    let run = Command::new(&bin).output().ok()?;
    let out = String::from_utf8_lossy(&run.stdout);
    let val = out.lines().rev().find_map(|l| l.trim().strip_prefix("Z80REF="))?;
    Some(normalize_hex(val))
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
    elf_rt: &ElfRuntime,
) -> TestResult {
    let tmp_dir = unique_tmp_dir(work_dir);
    let _ = std::fs::create_dir_all(&tmp_dir);

    let test_obj = tmp_dir.join(format!("{tag}.o"));
    let elf = tmp_dir.join(format!("{tag}.elf"));
    let bin = tmp_dir.join(format!("{tag}.bin"));

    // Compile to object only — the link step injects crt0 + compiler-rt
    // builtins + linker script explicitly so that _start, _halt, .bss
    // layout, and ___mulhi3 / ___udivhi3 / etc. are all resolved.
    let mut cmd = Command::new(clang.as_os_str());
    cmd.arg(format!("--target={}", target.triple()));
    cmd.arg(format!("-{}", opt.clang_flag()));
    cmd.arg("-c");
    cmd.arg("-nostdlib");
    cmd.arg("-ffreestanding");
    for flag in extra_flags {
        cmd.arg(flag);
    }
    cmd.arg(test_file.as_os_str());
    cmd.arg("-o");
    cmd.arg(test_obj.as_os_str());

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

    // Link: ld.lld -T <triple>.ld --gc-sections crt0.o test.o builtins/*.o
    let lld = clang.parent().unwrap().join("ld.lld");
    let mut link = Command::new(lld.as_os_str());
    link.arg("--gc-sections");
    link.arg("-T");
    link.arg(elf_rt.linker_script.as_os_str());
    link.arg(elf_rt.crt0_obj.as_os_str());
    link.arg(test_obj.as_os_str());
    for obj in &elf_rt.builtin_objs {
        link.arg(obj.as_os_str());
    }
    link.arg("-o");
    link.arg(elf.as_os_str());

    match run_cmd_timeout(&mut link, COMPILE_TIMEOUT) {
        Err(e) => {
            remove_tmp_dir(&tmp_dir);
            return TestResult::fatal(tag, format!("link {e}"));
        }
        Ok((code, _, stderr)) if code != 0 => {
            let err = extract_error(&stderr);
            remove_tmp_dir(&tmp_dir);
            return TestResult::fatal(tag, format!("link: {err}"));
        }
        _ => {}
    }

    // ELF → flat binary
    let objcopy = clang.parent().unwrap().join("llvm-objcopy");
    if let Err(e) = emulator::elf_to_bin(&objcopy, &elf, &bin) {
        remove_tmp_dir(&tmp_dir);
        return TestResult::fatal(tag, e);
    }

    // A flat binary larger than the 64 KB Z80/SM83 address space cannot be
    // loaded by z88dk-ticks (it rejects it with "Incorrect length", which the
    // emulate() path would surface as a cryptic "no register value" FATAL).
    // This is an environmental limit, not a test failure: the auto-generated
    // test_90/91_edge_* stress fixtures compile to a ~113 KB main() at -O0 and
    // only fit under -O1+ optimization.  Classify as SKIP so they still run
    // (and assert) at every opt level where they fit.
    if let Ok(meta) = std::fs::metadata(&bin) {
        if meta.len() > 0x1_0000 {
            remove_tmp_dir(&tmp_dir);
            return TestResult::skip(
                tag,
                format!("binary {} B exceeds 64 KB address space", meta.len()),
            );
        }
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
                    // ravn/llvm-z80#137: re-run capturing port-1 console output
                    // so multi-CHECK fixtures (test_90/91_edge_*) reveal WHICH
                    // sub-check failed (`FAIL @<line> got=.. exp=..`), not just
                    // the aggregate DE count.  Best-effort; only on failure.
                    let note = emulator::capture_port_output(&bin, target, &halt_addr, 1);
                    TestResult::fail(tag, format!("0x{got_padded}"), format!("0x{exp_padded}"))
                        .with_note(note)
                }
            }
        }
    };
    // Keep temp files on failure for debugging; clean up on pass.
    if result.is_pass() {
        remove_tmp_dir(&tmp_dir);
    }
    result
}

