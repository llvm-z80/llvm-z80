//! Test suite for elf2rel / rel2elf converter utilities.
//!
//! Six test groups (run in parallel):
//!   1. elf_roundtrip:     .c → ELF .o → elf2rel → .rel → rel2elf → .o → lld → run
//!   2. rel_roundtrip:     .c → sdasz80 .rel → rel2elf → .o → elf2rel → .rel → sdldz80 → run
//!   3. elf_crosslink:     Clang ELF .o (→elf2rel→.rel) + SDCC .rel → sdldz80 → run
//!   4. rel_crosslink:     SDCC .rel (→rel2elf→.o) + Clang ELF .o → lld → run
//!   5. elf_ar_roundtrip:  .a → elf2rel → .lib → rel2elf → .a → lld → run
//!   6. rel_ar_roundtrip:  .lib → rel2elf → .a → elf2rel → .lib → sdldz80 → run

use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use crate::config::{self, OptLevel, Paths, Target};
use crate::display;
use crate::emulator;
use crate::suite::*;

const COMPILE_TIMEOUT: u64 = 30;

pub struct UtilsConfig {
    pub target: Target,
    pub opt: OptLevel,
    pub pattern: Option<String>,
}

// ── Parallel execution ──────────────────────────────────────────────────────

struct GroupState {
    label: String,
    done: u32,
    total: u32,
    result: Option<SuiteResult>,
}

type SharedState = Arc<Mutex<Vec<GroupState>>>;

type GroupRunner = Box<dyn FnOnce(&Paths, SharedState, usize) + Send>;

struct GroupDef {
    label: String,
    runner: GroupRunner,
}

fn progress_callback(state: SharedState, idx: usize) -> OnResult {
    Box::new(move |_result, _reg| {
        let mut lock = state.lock().unwrap();
        lock[idx].done += 1;
    })
}

/// Run all 6 test groups sequentially, collecting results into a single SuiteResult.
/// Used by run_all to integrate utils as one suite.
pub fn run(paths: &Paths, config: &UtilsConfig, on_result: &mut OnResult) -> SuiteResult {
    let target = config.target;
    let opt = config.opt;
    let pattern = config.pattern.as_deref();
    let mut result = SuiteResult::default();

    result.merge(run_group_elf_roundtrip(paths, target, opt, pattern, on_result));
    result.merge(run_group_rel_roundtrip(paths, target, opt, pattern, on_result));
    result.merge(run_group_elf_crosslink(paths, target, opt, pattern, on_result));
    result.merge(run_group_rel_crosslink(paths, target, opt, pattern, on_result));
    result.merge(run_group_elf_ar_roundtrip(paths, target, opt, pattern, on_result));
    result.merge(run_group_rel_ar_roundtrip(paths, target, opt, pattern, on_result));

    result
}

/// Run all 6 test groups in parallel with a progress display.
/// Returns true if all tests passed.
pub fn run_parallel(paths: &Paths, config: &UtilsConfig) -> bool {
    let groups = build_groups(config);
    let num = groups.len();

    let state: SharedState = Arc::new(Mutex::new(
        groups.iter().map(|g| GroupState {
            label: g.label.clone(),
            done: 0,
            total: 0,
            result: None,
        }).collect()
    ));

    let paths = Arc::new(paths.clone());

    // Launch all groups in parallel
    let _handles: Vec<_> = groups
        .into_iter()
        .enumerate()
        .map(|(i, group)| {
            let state = Arc::clone(&state);
            let paths = Arc::clone(&paths);
            thread::spawn(move || {
                (group.runner)(&paths, state, i);
            })
        })
        .collect();

    // Display loop
    let tty = display::is_tty();
    let title = format!("{} elf2rel/rel2elf Utils Test", config.target.triple().to_uppercase());

    if tty {
        print!("\x1b[?25l"); // hide cursor
        print!("\x1b[1m{title}\x1b[0m\n");
        print!("========================\n");
        {
            let lock = state.lock().unwrap();
            for s in lock.iter() {
                print!("  \x1b[2m\u{22ef} {}  -\x1b[0m\n", s.label);
            }
        }
        print!("========================\n");
        let _ = io::stdout().flush();

        let total_lines = 2 + num + 1;

        loop {
            thread::sleep(Duration::from_millis(100));

            let lock = state.lock().unwrap();
            let finished = lock.iter().filter(|s| s.result.is_some()).count();

            print!("\x1b[{total_lines}A");
            print!("\x1b[1m{title}\x1b[0m\n");
            print!("========================\n");
            for s in lock.iter() {
                render_group_line_tty(s);
            }
            print!("========================\n");
            let _ = io::stdout().flush();

            if finished == num {
                drop(lock);
                break;
            }
            drop(lock);
        }

        print!("\x1b[?25h"); // show cursor
        let _ = io::stdout().flush();
    } else {
        let mut printed = vec![false; num];
        println!("{title}");
        println!("========================");

        loop {
            thread::sleep(Duration::from_millis(100));
            let lock = state.lock().unwrap();
            for (i, s) in lock.iter().enumerate() {
                if !printed[i] {
                    if let Some(ref r) = s.result {
                        print_group_line_plain(&s.label, r);
                        printed[i] = true;
                    }
                }
            }
            let finished = lock.iter().filter(|s| s.result.is_some()).count();
            drop(lock);
            if finished == num {
                break;
            }
        }
        println!("========================");
    }

    // Aggregate results
    let lock = state.lock().unwrap();
    let mut total_pass = 0u32;
    let mut total_fail = 0u32;
    let mut total_fatal = 0u32;
    let mut total_skip = 0u32;
    let mut total_all = 0u32;
    let mut all_ok = true;

    for s in lock.iter() {
        if let Some(ref r) = s.result {
            total_pass += r.pass;
            total_fail += r.fail;
            total_fatal += r.fatal;
            total_skip += r.skip;
            total_all += r.total;
            if !r.all_ok() {
                all_ok = false;
            }
        }
    }

    display::print_summary(total_all, total_pass, total_fail, total_fatal, total_skip, all_ok);

    if !all_ok {
        for s in lock.iter() {
            if let Some(ref r) = s.result {
                for t in &r.results {
                    match &t.outcome {
                        TestOutcome::Fail { got, expected } => {
                            println!("  FAIL  {}  (got {got}, expected {expected})", t.tag);
                        }
                        TestOutcome::Fatal { reason } => {
                            println!("  FATAL {}  ({reason})", t.tag);
                        }
                        _ => {}
                    }
                }
            }
        }
    }

    all_ok
}

fn render_group_line_tty(s: &GroupState) {
    print!("\x1b[2K");
    match &s.result {
        None => {
            if s.total > 0 {
                print!("  \x1b[2m\u{22ef} {}  [{}/{}]\x1b[0m\n", s.label, s.done, s.total);
            } else {
                print!("  \x1b[2m\u{22ef} {}  ...\x1b[0m\n", s.label);
            }
        }
        Some(r) => {
            if !r.all_ok() {
                print!("  \x1b[31m\u{2717}\x1b[0m {}  {}/{}", s.label, r.pass, r.total);
                if r.fail > 0 { print!("  \x1b[31mfail={}\x1b[0m", r.fail); }
                if r.fatal > 0 { print!("  \x1b[31mfatal={}\x1b[0m", r.fatal); }
                println!();
            } else {
                print!("  \x1b[32m\u{2713}\x1b[0m {}  {}/{}", s.label, r.pass, r.total);
                if r.skip > 0 { print!("  \x1b[33mskip={}\x1b[0m", r.skip); }
                println!();
            }
        }
    }
}

fn print_group_line_plain(label: &str, r: &SuiteResult) {
    if !r.all_ok() {
        print!("  x {label}  {}/{}", r.pass, r.total);
        if r.fail > 0 { print!("  fail={}", r.fail); }
        if r.fatal > 0 { print!("  fatal={}", r.fatal); }
    } else {
        print!("  ok {label}  {}/{}", r.pass, r.total);
        if r.skip > 0 { print!("  skip={}", r.skip); }
    }
    println!();
    let _ = io::stdout().flush();
}

fn build_groups(config: &UtilsConfig) -> Vec<GroupDef> {
    let target = config.target;
    let opt = config.opt;
    let pattern = config.pattern.clone();

    let mut groups = Vec::new();

    macro_rules! add_group {
        ($label:expr, $func:ident, $pat:expr) => {{
            let pat = $pat.clone();
            groups.push(GroupDef {
                label: $label.into(),
                runner: Box::new(move |paths, state, idx| {
                    let mut cb = progress_callback(state.clone(), idx);
                    let result = $func(paths, target, opt, pat.as_deref(), &mut cb);
                    state.lock().unwrap()[idx].total = result.total;
                    state.lock().unwrap()[idx].result = Some(result);
                }),
            });
        }};
    }

    add_group!("elf roundtrip", run_group_elf_roundtrip, pattern);
    add_group!("rel roundtrip", run_group_rel_roundtrip, pattern);
    add_group!("elf2rel crosslink", run_group_elf_crosslink, pattern);
    add_group!("rel2elf crosslink", run_group_rel_crosslink, pattern);
    add_group!("elf archive roundtrip", run_group_elf_ar_roundtrip, pattern);
    add_group!("rel archive roundtrip", run_group_rel_ar_roundtrip, pattern);

    groups
}

// ── Helpers ─────────────────────────────────────────────────────────────────

fn run_elf2rel(input: &Path, output: &Path) -> Result<(), String> {
    let status = Command::new("elf2rel")
        .arg(input)
        .arg(output)
        .stdout(std::process::Stdio::null())
        .stderr(std::process::Stdio::piped())
        .status()
        .map_err(|e| format!("elf2rel: {e}"))?;
    if status.success() { Ok(()) } else { Err("elf2rel failed".into()) }
}

fn run_rel2elf(input: &Path, output: &Path) -> Result<(), String> {
    let status = Command::new("rel2elf")
        .arg(input)
        .arg(output)
        .stdout(std::process::Stdio::null())
        .stderr(std::process::Stdio::piped())
        .status()
        .map_err(|e| format!("rel2elf: {e}"))?;
    if status.success() { Ok(()) } else { Err("rel2elf failed".into()) }
}

fn clang_to_elf(
    clang: &Path, src: &Path, out: &Path, target: Target, opt: OptLevel,
    extra_flags: &[String],
) -> Result<(), String> {
    let mut cmd = Command::new(clang);
    cmd.arg(format!("--target={}", target.triple()));
    cmd.arg(format!("-{}", opt.clang_flag()));
    cmd.args(["-c"]);
    for flag in extra_flags { cmd.arg(flag); }
    cmd.arg(src).arg("-o").arg(out);
    match run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
        Err(e) => Err(format!("clang -c: {e}")),
        Ok((code, _, stderr)) if code != 0 => Err(extract_error(&stderr)),
        _ => Ok(()),
    }
}

fn link_with_clang(
    clang: &Path, target: Target, objs: &[&Path], extra_libs: &[&Path], output: &Path,
) -> Result<(), String> {
    let mut cmd = Command::new(clang);
    cmd.arg(format!("--target={}", target.triple()));
    for obj in objs { cmd.arg(obj); }
    for lib in extra_libs { cmd.arg(lib); }
    cmd.arg("-o").arg(output);
    match run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
        Err(e) => Err(format!("clang link: {e}")),
        Ok((code, _, stderr)) if code != 0 => Err(extract_error(&stderr)),
        _ => Ok(()),
    }
}

fn convert_sdcc_lib(target: Target, output: &Path) -> Result<Option<PathBuf>, String> {
    let sdcc_lib = match config::find_sdcc_lib(target) {
        Some(lib) => lib,
        None => return Ok(None),
    };
    let status = Command::new("rel2elf")
        .arg(&sdcc_lib).arg(output)
        .stdout(std::process::Stdio::null())
        .stderr(std::process::Stdio::piped())
        .status()
        .map_err(|e| format!("rel2elf .lib→.a: {e}"))?;
    if status.success() { Ok(Some(output.to_path_buf())) } else { Err("rel2elf .lib→.a failed".into()) }
}

fn run_elf_binary(clang: &Path, elf: &Path, source: &str, target: Target, tag: &str) -> TestResult {
    let objcopy = clang.parent().unwrap().join("llvm-objcopy");
    let bin = elf.with_extension("bin");
    if let Err(e) = emulator::elf_to_bin(&objcopy, elf, &bin) {
        return TestResult::fatal(tag, e);
    }
    let halt_addr = match emulator::halt_addr_from_elf(
        &clang.parent().unwrap().join("llvm-nm"), elf) {
        Some(addr) => addr,
        None => return TestResult::fatal(tag, "_halt symbol not found in ELF"),
    };
    match emulator::emulate(&bin, target, &halt_addr) {
        Err(e) => TestResult::fatal(tag, e),
        Ok(got) => {
            let expected = emulator::parse_expected(source);
            match emulator::check_result(&got, &expected) {
                Ok(()) => TestResult::pass(tag, format!("0x{got}")),
                Err((g, e)) => TestResult::fail(tag, format!("0x{g}"), format!("0x{e}")),
            }
        }
    }
}

fn sdcc_to_rel(
    sdcc_src: &Path, asm_out: &Path, rel_out: &Path, target: Target, _tag: &str,
) -> Result<(), String> {
    let mut cmd = Command::new("sdcc");
    cmd.args([target.sdcc_flag(), "--std-c11", "-S"]);
    cmd.arg(sdcc_src).arg("-o").arg(asm_out);
    match run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
        Err(e) => return Err(format!("SDCC: {e}")),
        Ok((code, _, stderr)) if code != 0 => {
            let err = stderr.lines().next().unwrap_or("error").trim();
            return Err(format!("SDCC: {err}"));
        }
        _ => {}
    }
    let status = Command::new(target.assembler())
        .args(["-g", "-o"]).arg(rel_out).arg(asm_out)
        .stdout(std::process::Stdio::null()).stderr(std::process::Stdio::null())
        .status().map_err(|e| format!("assembler: {e}"))?;
    if !status.success() { return Err("SDCC assemble failed".into()); }
    Ok(())
}

fn link_rels(
    target: Target, out_base: &Path, rels: &[&Path], paths: &Paths,
) -> Result<PathBuf, String> {
    link_rels_with_custom_lib(target, out_base, rels, paths, None)
}

fn link_rels_with_custom_lib(
    target: Target, out_base: &Path, rels: &[&Path], paths: &Paths,
    custom_rt_lib: Option<&Path>,
) -> Result<PathBuf, String> {
    let ihx = out_base.with_extension("ihx");
    let crt0 = paths.crt0(target);
    let sdcc_lib = config::find_sdcc_lib(target);

    let mut cmd = Command::new(target.linker());
    cmd.args(["-m", "-i"]).arg(out_base).arg(&crt0);
    for rel in rels { cmd.arg(rel); }

    // Use custom runtime lib if provided, otherwise use default
    let rt_lib = match custom_rt_lib {
        Some(lib) => lib.to_path_buf(),
        None => paths.rt_lib(target),
    };
    if rt_lib.exists() {
        let lib_dir = rt_lib.parent().unwrap();
        let lib_name = rt_lib.file_stem().unwrap();
        cmd.arg("-k").arg(lib_dir);
        cmd.arg("-l").arg(lib_name);
    }
    if let Some(ref lib) = sdcc_lib {
        let lib_dir = lib.parent().unwrap();
        let lib_name = lib.file_stem().unwrap();
        cmd.arg("-k").arg(lib_dir);
        cmd.arg("-l").arg(lib_name);
    }
    cmd.stdout(std::process::Stdio::null()).stderr(std::process::Stdio::null());

    let _status = cmd.status().map_err(|e| format!("linker: {e}"))?;
    if !ihx.exists() { return Err("link failed".into()); }
    Ok(ihx)
}

fn run_ihx_binary(ihx: &Path, source: &str, target: Target, tag: &str) -> TestResult {
    let bin = ihx.with_extension("bin");
    if let Err(e) = emulator::makebin(ihx, &bin) {
        return TestResult::fatal(tag, e);
    }
    let map_file = ihx.with_extension("map");
    let halt_addr = match emulator::halt_addr_from_map(&map_file) {
        Some(addr) => addr,
        None => return TestResult::fatal(tag, "_halt symbol not found in map file"),
    };
    match emulator::emulate(&bin, target, &halt_addr) {
        Err(e) => TestResult::fatal(tag, e),
        Ok(got) => {
            let expected = emulator::parse_expected(source);
            match emulator::check_result(&got, &expected) {
                Ok(()) => TestResult::pass(tag, format!("0x{got}")),
                Err((g, e)) => TestResult::fail(tag, format!("0x{g}"), format!("0x{e}")),
            }
        }
    }
}

fn clang_to_rel(
    clang: &Path, src: &Path, _asm_out: &Path, rel_out: &Path, target: Target, opt: OptLevel, _tag: &str,
    extra_flags: &[String],
) -> Result<(), String> {
    let mut cmd = Command::new(clang);
    cmd.arg(format!("--target={}", target.sdcc_triple()));
    cmd.arg(format!("-{}", opt.clang_flag()));
    cmd.arg("-c");
    for flag in extra_flags { cmd.arg(flag); }
    cmd.arg(src).arg("-o").arg(rel_out);
    match run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
        Err(e) => return Err(format!("clang -c: {e}")),
        Ok((code, _, stderr)) if code != 0 => return Err(extract_error(&stderr)),
        _ => {}
    }
    Ok(())
}

// ── Group 1: ELF roundtrip ─────────────────────────────────────────────────

fn run_group_elf_roundtrip(
    paths: &Paths, target: Target, opt: OptLevel, pattern: Option<&str>,
    cb: &mut OnResult,
) -> SuiteResult {
    let test_dir = paths.clang_test_dir();
    let clang = paths.clang();
    let tests = discover_tests(&test_dir, "test_", "c");
    let reg_name = target.reg_name();
    let mut result = SuiteResult::default();

    for test_file in &tests {
        let name = test_file.file_stem().unwrap().to_string_lossy().to_string();
        if let Some(pat) = pattern {
            if !name.contains(pat) { continue; }
        }

        let source = std::fs::read_to_string(test_file).unwrap_or_default();
        if let Some(reason) = check_skip_c(&source, target, &[], "") {
            let tag = format!("{name}_elf_rt");
            result.add(TestResult::skip(&tag, &reason), cb, reg_name);
            continue;
        }

        let tag = format!("{name}_elf_rt");
        let r = test_elf_roundtrip(&clang, test_file, &tag, target, opt, &source, &test_dir);
        result.add(r, cb, reg_name);
    }

    result
}

fn test_elf_roundtrip(
    clang: &Path, src: &Path, tag: &str, target: Target, opt: OptLevel,
    source: &str, work_dir: &Path,
) -> TestResult {
    let tmp = unique_tmp_dir(work_dir);
    let _ = std::fs::create_dir_all(&tmp);

    let elf_o = tmp.join(format!("{tag}.o"));
    let extra_flags = parse_extra_flags_c(source);
    if let Err(e) = clang_to_elf(clang, src, &elf_o, target, opt, &extra_flags) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, e);
    }

    let rel = tmp.join(format!("{tag}.rel"));
    if let Err(e) = run_elf2rel(&elf_o, &rel) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, format!("elf→rel: {e}"));
    }

    let rt_o = tmp.join(format!("{tag}_rt.o"));
    if let Err(e) = run_rel2elf(&rel, &rt_o) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, format!("rel→elf: {e}"));
    }

    let elf = tmp.join(format!("{tag}.elf"));
    if let Err(e) = link_with_clang(clang, target, &[rt_o.as_path()], &[], &elf) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, format!("link: {e}"));
    }

    let r = run_elf_binary(clang, &elf, source, target, tag);
    remove_tmp_dir(&tmp);
    r
}

// ── Group 2: REL roundtrip ─────────────────────────────────────────────────

fn run_group_rel_roundtrip(
    paths: &Paths, target: Target, opt: OptLevel, pattern: Option<&str>,
    cb: &mut OnResult,
) -> SuiteResult {
    let test_dir = paths.clang_test_dir();
    let clang = paths.clang();
    let tests = discover_tests(&test_dir, "test_", "c");
    let reg_name = target.reg_name();
    let mut result = SuiteResult::default();

    for test_file in &tests {
        let name = test_file.file_stem().unwrap().to_string_lossy().to_string();
        if let Some(pat) = pattern {
            if !name.contains(pat) { continue; }
        }

        let source = std::fs::read_to_string(test_file).unwrap_or_default();
        if let Some(reason) = check_skip_c(&source, target, &["-fno-integrated-as"], "") {
            let tag = format!("{name}_rel_rt");
            result.add(TestResult::skip(&tag, &reason), cb, reg_name);
            continue;
        }

        let tag = format!("{name}_rel_rt");
        let r = test_rel_roundtrip(&clang, test_file, &tag, target, opt, &source, &test_dir, paths);
        result.add(r, cb, reg_name);
    }

    result
}

fn test_rel_roundtrip(
    clang: &Path, src: &Path, tag: &str, target: Target, opt: OptLevel,
    source: &str, work_dir: &Path, paths: &Paths,
) -> TestResult {
    let tmp = unique_tmp_dir(work_dir);
    let _ = std::fs::create_dir_all(&tmp);

    let asm = tmp.join(format!("{tag}.s"));
    let rel = tmp.join(format!("{tag}.rel"));
    let extra_flags = parse_extra_flags_c(source);
    if let Err(e) = clang_to_rel(clang, src, &asm, &rel, target, opt, tag, &extra_flags) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, e);
    }

    let elf_o = tmp.join(format!("{tag}.o"));
    if let Err(e) = run_rel2elf(&rel, &elf_o) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, format!("rel→elf: {e}"));
    }

    let rt_rel = tmp.join(format!("{tag}_rt.rel"));
    if let Err(e) = run_elf2rel(&elf_o, &rt_rel) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, format!("elf→rel: {e}"));
    }

    let out_base = tmp.join(tag);
    let ihx = match link_rels(target, &out_base, &[rt_rel.as_path()], paths) {
        Ok(ihx) => ihx,
        Err(e) => { remove_tmp_dir(&tmp); return TestResult::fatal(tag, e); }
    };

    let r = run_ihx_binary(&ihx, source, target, tag);
    remove_tmp_dir(&tmp);
    r
}

// ── Group 3: elf2rel crosslink ──────────────────────────────────────────────

fn run_group_elf_crosslink(
    paths: &Paths, target: Target, opt: OptLevel, pattern: Option<&str>,
    cb: &mut OnResult,
) -> SuiteResult {
    let test_dir = paths.sdcc_test_dir();
    let clang = paths.clang();
    let test_names = discover_sdcc_test_names(&test_dir);
    let reg_name = target.reg_name();
    let mut result = SuiteResult::default();

    for test_name in &test_names {
        if let Some(pat) = pattern {
            if !test_name.contains(pat) { continue; }
        }

        let clang_src = test_dir.join(format!("{test_name}_clang.c"));
        let sdcc_src = test_dir.join(format!("{test_name}_sdcc.c"));
        if !clang_src.exists() || !sdcc_src.exists() { continue; }

        let tag = format!("{test_name}_elf_cross");
        let r = test_elf_crosslink(
            &clang, &clang_src, &sdcc_src, &tag, test_name,
            target, opt, &test_dir, paths,
        );
        result.add(r, cb, reg_name);
    }

    result
}

fn test_elf_crosslink(
    clang: &Path, clang_src: &Path, sdcc_src: &Path, tag: &str, test_name: &str,
    target: Target, opt: OptLevel, work_dir: &Path, paths: &Paths,
) -> TestResult {
    let tmp = unique_tmp_dir(work_dir);
    let _ = std::fs::create_dir_all(&tmp);

    let sdcc_asm = tmp.join(format!("{tag}_sdcc.asm"));
    let sdcc_rel = tmp.join(format!("{tag}_sdcc.rel"));
    if let Err(e) = sdcc_to_rel(sdcc_src, &sdcc_asm, &sdcc_rel, target, tag) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, e);
    }

    let clang_elf = tmp.join(format!("{tag}_clang.o"));
    let clang_source = std::fs::read_to_string(clang_src).unwrap_or_default();
    let clang_extra = parse_extra_flags_c(&clang_source);
    if let Err(e) = clang_to_elf(clang, clang_src, &clang_elf, target, opt, &clang_extra) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, e);
    }
    let clang_rel = tmp.join(format!("{tag}_clang.rel"));
    if let Err(e) = run_elf2rel(&clang_elf, &clang_rel) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, format!("elf2rel: {e}"));
    }

    let is_reverse = test_name.contains("reverse");
    let (main_rel, lib_rel) = if is_reverse {
        (sdcc_rel.as_path(), clang_rel.as_path())
    } else {
        (clang_rel.as_path(), sdcc_rel.as_path())
    };

    let out_base = tmp.join(tag);
    let ihx = match link_rels(target, &out_base, &[main_rel, lib_rel], paths) {
        Ok(ihx) => ihx,
        Err(e) => { remove_tmp_dir(&tmp); return TestResult::fatal(tag, e); }
    };

    let source_file = if is_reverse { sdcc_src } else { clang_src };
    let source = std::fs::read_to_string(source_file).unwrap_or_default();
    let r = run_ihx_binary(&ihx, &source, target, tag);
    remove_tmp_dir(&tmp);
    r
}

// ── Group 4: rel2elf crosslink ──────────────────────────────────────────────

fn run_group_rel_crosslink(
    paths: &Paths, target: Target, opt: OptLevel, pattern: Option<&str>,
    cb: &mut OnResult,
) -> SuiteResult {
    let test_dir = paths.sdcc_test_dir();
    let clang = paths.clang();
    let test_names = discover_sdcc_test_names(&test_dir);
    let reg_name = target.reg_name();
    let mut result = SuiteResult::default();

    for test_name in &test_names {
        if let Some(pat) = pattern {
            if !test_name.contains(pat) { continue; }
        }

        let clang_src = test_dir.join(format!("{test_name}_clang.c"));
        let sdcc_src = test_dir.join(format!("{test_name}_sdcc.c"));
        if !clang_src.exists() || !sdcc_src.exists() { continue; }

        let tag = format!("{test_name}_rel_cross");
        let r = test_rel_crosslink(
            &clang, &clang_src, &sdcc_src, &tag, test_name,
            target, opt, &test_dir,
        );
        result.add(r, cb, reg_name);
    }

    result
}

fn test_rel_crosslink(
    clang: &Path, clang_src: &Path, sdcc_src: &Path, tag: &str, test_name: &str,
    target: Target, opt: OptLevel, work_dir: &Path,
) -> TestResult {
    let tmp = unique_tmp_dir(work_dir);
    let _ = std::fs::create_dir_all(&tmp);

    let clang_o = tmp.join(format!("{tag}_clang.o"));
    let clang_source = std::fs::read_to_string(clang_src).unwrap_or_default();
    let clang_extra = parse_extra_flags_c(&clang_source);
    if let Err(e) = clang_to_elf(clang, clang_src, &clang_o, target, opt, &clang_extra) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, e);
    }

    let sdcc_asm = tmp.join(format!("{tag}_sdcc.asm"));
    let sdcc_rel = tmp.join(format!("{tag}_sdcc.rel"));
    if let Err(e) = sdcc_to_rel(sdcc_src, &sdcc_asm, &sdcc_rel, target, tag) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, e);
    }
    let sdcc_o = tmp.join(format!("{tag}_sdcc.o"));
    if let Err(e) = run_rel2elf(&sdcc_rel, &sdcc_o) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, format!("rel2elf: {e}"));
    }

    let sdcc_rt_a = tmp.join("sdcc_rt.a");
    let sdcc_rt = match convert_sdcc_lib(target, &sdcc_rt_a) {
        Ok(lib) => lib,
        Err(e) => { remove_tmp_dir(&tmp); return TestResult::fatal(tag, format!("sdcc runtime: {e}")); }
    };
    let extra_libs: Vec<&Path> = sdcc_rt.iter().map(|p| p.as_path()).collect();

    let is_reverse = test_name.contains("reverse");
    let (main_o, lib_o) = if is_reverse {
        (sdcc_o.as_path(), clang_o.as_path())
    } else {
        (clang_o.as_path(), sdcc_o.as_path())
    };

    let elf = tmp.join(format!("{tag}.elf"));
    if let Err(e) = link_with_clang(clang, target, &[main_o, lib_o], &extra_libs, &elf) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, format!("link: {e}"));
    }

    let source_file = if is_reverse { sdcc_src } else { clang_src };
    let source = std::fs::read_to_string(source_file).unwrap_or_default();
    let r = run_elf_binary(clang, &elf, &source, target, tag);
    remove_tmp_dir(&tmp);
    r
}

// ── Group 5: ELF archive roundtrip ─────────────────────────────────────────
// .a (build rt) → elf2rel → .lib → rel2elf → .a → link test programs via lld

fn run_group_elf_ar_roundtrip(
    paths: &Paths, target: Target, opt: OptLevel, pattern: Option<&str>,
    cb: &mut OnResult,
) -> SuiteResult {
    let test_dir = paths.clang_test_dir();
    let clang = paths.clang();
    let tests = discover_tests(&test_dir, "test_", "c");
    let reg_name = target.reg_name();
    let mut result = SuiteResult::default();

    // Roundtrip the runtime archive: .a (ELF) → elf2rel → .lib → rel2elf → .a
    let tmp_ar = unique_tmp_dir(&test_dir);
    let _ = std::fs::create_dir_all(&tmp_ar);

    // Find the ELF .a runtime (built by clang driver toolchain)
    // The clang driver auto-links z80_rt, but we need the .a for archive roundtrip.
    // Convert the .lib to .a first, then roundtrip it.
    let rt_lib_path = paths.rt_lib(target);
    let roundtripped_a = if rt_lib_path.exists() {
        let initial_a = tmp_ar.join("initial_rt.a");
        match run_rel2elf(&rt_lib_path, &initial_a) {
            Ok(()) => {
                let rt_lib = tmp_ar.join("roundtrip_rt.lib");
                match run_elf2rel(&initial_a, &rt_lib) {
                    Ok(()) => {
                        let rt_a = tmp_ar.join("roundtrip_rt.a");
                        match run_rel2elf(&rt_lib, &rt_a) {
                            Ok(()) => Some(rt_a),
                            Err(e) => {
                                for test_file in &tests {
                                    let name = test_file.file_stem().unwrap().to_string_lossy().to_string();
                                    if let Some(pat) = pattern { if !name.contains(pat) { continue; } }
                                    let tag = format!("{name}_elf_ar");
                                    result.add(TestResult::fatal(&tag, format!("ar roundtrip: {e}")), cb, reg_name);
                                }
                                remove_tmp_dir(&tmp_ar);
                                return result;
                            }
                        }
                    }
                    Err(e) => {
                        for test_file in &tests {
                            let name = test_file.file_stem().unwrap().to_string_lossy().to_string();
                            if let Some(pat) = pattern { if !name.contains(pat) { continue; } }
                            let tag = format!("{name}_elf_ar");
                            result.add(TestResult::fatal(&tag, format!("elf2rel .a→.lib: {e}")), cb, reg_name);
                        }
                        remove_tmp_dir(&tmp_ar);
                        return result;
                    }
                }
            }
            Err(e) => {
                for test_file in &tests {
                    let name = test_file.file_stem().unwrap().to_string_lossy().to_string();
                    if let Some(pat) = pattern { if !name.contains(pat) { continue; } }
                    let tag = format!("{name}_elf_ar");
                    result.add(TestResult::fatal(&tag, format!("rel2elf .lib→.a: {e}")), cb, reg_name);
                }
                remove_tmp_dir(&tmp_ar);
                return result;
            }
        }
    } else {
        None
    };

    for test_file in &tests {
        let name = test_file.file_stem().unwrap().to_string_lossy().to_string();
        if let Some(pat) = pattern {
            if !name.contains(pat) { continue; }
        }

        let source = std::fs::read_to_string(test_file).unwrap_or_default();
        if let Some(reason) = check_skip_c(&source, target, &[], "") {
            let tag = format!("{name}_elf_ar");
            result.add(TestResult::skip(&tag, &reason), cb, reg_name);
            continue;
        }

        let tag = format!("{name}_elf_ar");
        let r = test_elf_ar_roundtrip(
            &clang, test_file, &tag, target, opt, &source, &test_dir,
            roundtripped_a.as_deref(),
        );
        result.add(r, cb, reg_name);
    }

    remove_tmp_dir(&tmp_ar);
    result
}

fn test_elf_ar_roundtrip(
    clang: &Path, src: &Path, tag: &str, target: Target, opt: OptLevel,
    source: &str, work_dir: &Path, roundtripped_a: Option<&Path>,
) -> TestResult {
    let tmp = unique_tmp_dir(work_dir);
    let _ = std::fs::create_dir_all(&tmp);

    let elf_o = tmp.join(format!("{tag}.o"));
    let extra_flags = parse_extra_flags_c(source);
    if let Err(e) = clang_to_elf(clang, src, &elf_o, target, opt, &extra_flags) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, e);
    }

    let elf = tmp.join(format!("{tag}.elf"));
    let extra_libs: Vec<&Path> = roundtripped_a.into_iter().collect();
    if let Err(e) = link_with_clang(clang, target, &[elf_o.as_path()], &extra_libs, &elf) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, format!("link: {e}"));
    }

    let r = run_elf_binary(clang, &elf, source, target, tag);
    remove_tmp_dir(&tmp);
    r
}

// ── Group 6: REL archive roundtrip ─────────────────────────────────────────
// .lib (build rt) → rel2elf → .a → elf2rel → .lib → link test programs via sdldz80

fn run_group_rel_ar_roundtrip(
    paths: &Paths, target: Target, opt: OptLevel, pattern: Option<&str>,
    cb: &mut OnResult,
) -> SuiteResult {
    let test_dir = paths.clang_test_dir();
    let clang = paths.clang();
    let tests = discover_tests(&test_dir, "test_", "c");
    let reg_name = target.reg_name();
    let mut result = SuiteResult::default();

    let tmp_ar = unique_tmp_dir(&test_dir);
    let _ = std::fs::create_dir_all(&tmp_ar);

    let rt_lib_path = paths.rt_lib(target);
    let roundtripped_lib = if rt_lib_path.exists() {
        let rt_a = tmp_ar.join("roundtrip_rt.a");
        match run_rel2elf(&rt_lib_path, &rt_a) {
            Ok(()) => {
                let rt_lib = tmp_ar.join("roundtrip_rt.lib");
                match run_elf2rel(&rt_a, &rt_lib) {
                    Ok(()) => Some(rt_lib),
                    Err(e) => {
                        for test_file in &tests {
                            let name = test_file.file_stem().unwrap().to_string_lossy().to_string();
                            if let Some(pat) = pattern { if !name.contains(pat) { continue; } }
                            let tag = format!("{name}_rel_ar");
                            result.add(TestResult::fatal(&tag, format!("elf2rel .a→.lib: {e}")), cb, reg_name);
                        }
                        remove_tmp_dir(&tmp_ar);
                        return result;
                    }
                }
            }
            Err(e) => {
                for test_file in &tests {
                    let name = test_file.file_stem().unwrap().to_string_lossy().to_string();
                    if let Some(pat) = pattern { if !name.contains(pat) { continue; } }
                    let tag = format!("{name}_rel_ar");
                    result.add(TestResult::fatal(&tag, format!("rel2elf .lib→.a: {e}")), cb, reg_name);
                }
                remove_tmp_dir(&tmp_ar);
                return result;
            }
        }
    } else {
        None
    };

    for test_file in &tests {
        let name = test_file.file_stem().unwrap().to_string_lossy().to_string();
        if let Some(pat) = pattern {
            if !name.contains(pat) { continue; }
        }

        let source = std::fs::read_to_string(test_file).unwrap_or_default();
        if let Some(reason) = check_skip_c(&source, target, &["-fno-integrated-as"], "") {
            let tag = format!("{name}_rel_ar");
            result.add(TestResult::skip(&tag, &reason), cb, reg_name);
            continue;
        }

        let tag = format!("{name}_rel_ar");
        let r = test_rel_ar_roundtrip(
            &clang, test_file, &tag, target, opt, &source, &test_dir, paths,
            roundtripped_lib.as_deref(),
        );
        result.add(r, cb, reg_name);
    }

    remove_tmp_dir(&tmp_ar);
    result
}

fn test_rel_ar_roundtrip(
    clang: &Path, src: &Path, tag: &str, target: Target, opt: OptLevel,
    source: &str, work_dir: &Path, paths: &Paths, roundtripped_lib: Option<&Path>,
) -> TestResult {
    let tmp = unique_tmp_dir(work_dir);
    let _ = std::fs::create_dir_all(&tmp);

    // Compile to .rel via clang + sdasz80
    let asm = tmp.join(format!("{tag}.s"));
    let rel = tmp.join(format!("{tag}.rel"));
    let extra_flags = parse_extra_flags_c(source);
    if let Err(e) = clang_to_rel(clang, src, &asm, &rel, target, opt, tag, &extra_flags) {
        remove_tmp_dir(&tmp); return TestResult::fatal(tag, e);
    }

    // Link with sdldz80 using roundtripped .lib
    let out_base = tmp.join(tag);
    let ihx = match link_rels_with_custom_lib(target, &out_base, &[rel.as_path()], paths, roundtripped_lib) {
        Ok(ihx) => ihx,
        Err(e) => { remove_tmp_dir(&tmp); return TestResult::fatal(tag, e); }
    };

    let r = run_ihx_binary(&ihx, source, target, tag);
    remove_tmp_dir(&tmp);
    r
}

// ── Shared utilities ────────────────────────────────────────────────────────

pub(crate) fn discover_sdcc_test_names(test_dir: &Path) -> Vec<String> {
    let mut names: Vec<String> = std::fs::read_dir(test_dir)
        .into_iter()
        .flatten()
        .filter_map(|e| e.ok())
        .filter_map(|e| {
            let name = e.file_name().to_string_lossy().to_string();
            if name.starts_with("test_") && name.ends_with("_clang.c") {
                Some(name.strip_suffix("_clang.c")?.to_string())
            } else {
                None
            }
        })
        .collect();
    names.sort();
    names
}
