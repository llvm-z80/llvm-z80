//! GCC C torture suite runner.
//!
//! Two tiers over the sources in `z80-utils/vendor/gcc-torture` (a submodule):
//!
//! * `compile` -- 2003 tests with no `main()`.  The assertion is only that
//!   clang accepts the input and emits an object, so this tier needs neither
//!   the linker nor the emulator and is where compiler crashes surface most
//!   cheaply.
//! * `execute` -- 1879 self-checking tests.  `main()` returns 0 on success and
//!   calls `abort()` on failure, so the expected result register value is
//!   always 0x0000 and no `expect` directive is needed.  `torture/shim` supplies
//!   `abort`/`exit`/`link_error`, which steer into crt0's `_halt` with the
//!   failure encoded in the return register (0xDEAD for abort, the status for
//!   exit).
//!
//! Unlike the other suites this one has no expected-failure mechanism.
//! `torture/manifest.txt` skips only what the target structurally cannot do;
//! a backend bug keeps running and keeps failing until it is fixed.  The suite
//! is therefore red for as long as any bug is outstanding, which is the point,
//! and why it is not part of `run_all`.

use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::collections::BTreeSet;
use std::sync::Mutex;
use std::sync::atomic::{AtomicUsize, Ordering};

use crate::config::{OptLevel, Paths, Target};
use crate::display;
use crate::emulator;
use crate::runtime::{self, ElfRuntime};
use crate::suite::{remove_tmp_dir, run_cmd_timeout, unique_tmp_dir};
use crate::torture_data::{self, Manifest};

const COMPILE_TIMEOUT: u64 = 20;
const LINK_TIMEOUT: u64 = 20;
/// Default emulator budget, which also sets the cycle budget. Wide enough for
/// the slowest tests that legitimately finish: arith-rand-ll runs 10000 rounds
/// of 64-bit div/mod and needs some 9.4e9 cycles, 920501-6 sieves three large
/// primes with 64-bit modulo and needs 2.7e9. A tighter budget reported those
/// as TIMEOUT, which reads as a failure and would have cost the coverage they
/// carry. The price is that a test that really does hang now burns this long
/// before it is reported; tests run in parallel, so the effect on a full run
/// is small.
pub const EMU_TIMEOUT: u64 = 30;

/// Stack budget advertised to the tests, in bytes. The stack starts at the top
/// of the address space and grows down toward .bss, so this is a self-imposed
/// limit rather than a hardware one.
const STACK_SIZE: u32 = 2048;

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum Tier {
    Compile,
    Execute,
}

impl Tier {
    pub fn dir(&self) -> &'static str {
        match self {
            Tier::Compile => "compile",
            Tier::Execute => "execute",
        }
    }

    pub fn parse(s: &str) -> Option<Vec<Tier>> {
        match s {
            "compile" => Some(vec![Tier::Compile]),
            "execute" => Some(vec![Tier::Execute]),
            "all" => Some(vec![Tier::Compile, Tier::Execute]),
            _ => None,
        }
    }
}

pub struct TortureConfig {
    pub target: Target,
    pub opt_levels: Vec<OptLevel>,
    pub tiers: Vec<Tier>,
    pub jobs: usize,
    pub pattern: Option<String>,
    pub freestanding: bool,
    pub verify: bool,
    /// Run only the tests the manifest skips, and report the ones that now
    /// pass.  Skipped tests are otherwise never executed, so without this the
    /// manifest silently rots and a wrong `skip=` hides a working test forever.
    pub run_skipped: bool,
    pub list_failures: Option<PathBuf>,
    pub std: String,
    /// Seconds a test may run on the emulator before it counts as hung.
    pub emu_timeout: u64,
}

pub enum Verdict {
    Pass,
    /// Ran to _halt with a non-zero result register.  0xDEAD means the test
    /// called abort(); anything else is the value main() or exit() produced.
    Fail { got: String },
    /// Compiler crash, LLVM ERROR, or a MachineVerifier complaint.  Always the
    /// most serious bucket, reported first.
    Ice { detail: String },
    /// A failure the manifest attributes to clang rather than to this backend.
    /// It stays in the run rather than being skipped, so it turns green by
    /// itself the day clang fixes the bug.
    Clang { detail: String },
    /// The test carries a `dg-error` and was rejected, which is what upstream
    /// expects of it. Not a problem, but not a clean pass either.
    Xfail { detail: String },
    /// Clang rejected the source with an ordinary diagnostic.
    CompileFail { detail: String },
    LinkFail { detail: String },
    /// The link failed only on `link_error`/`link_error0..7`, the torture
    /// suite's marker for a call the optimizer was supposed to delete. The
    /// code is correct; an optimization that GCC performs did not happen.
    Optim { detail: String },
    Timeout,
    TooBig { detail: String },
    Skip { reason: String },
}

impl Verdict {
    fn is_pass(&self) -> bool {
        matches!(self, Verdict::Pass)
    }
    /// Outcomes that need no investigation.
    fn is_ok(&self) -> bool {
        matches!(self, Verdict::Pass | Verdict::Xfail { .. } | Verdict::Skip { .. })
    }
    fn label(&self) -> &'static str {
        match self {
            Verdict::Pass => "PASS",
            Verdict::Fail { .. } => "FAIL",
            Verdict::Ice { .. } => "ICE",
            Verdict::Clang { .. } => "CLANG",
            Verdict::Xfail { .. } => "XFAIL",
            Verdict::CompileFail { .. } => "COMPILE",
            Verdict::LinkFail { .. } => "LINK",
            Verdict::Optim { .. } => "OPTIM",
            Verdict::Timeout => "TIMEOUT",
            Verdict::TooBig { .. } => "TOOBIG",
            Verdict::Skip { .. } => "SKIP",
        }
    }
    fn detail(&self) -> String {
        match self {
            Verdict::Pass => String::new(),
            Verdict::Fail { got } => format!("got {got}"),
            Verdict::Ice { detail }
            | Verdict::Clang { detail }
            | Verdict::Xfail { detail }
            | Verdict::CompileFail { detail }
            | Verdict::LinkFail { detail }
            | Verdict::Optim { detail } => detail.clone(),
            Verdict::Timeout => "did not reach _halt in time".to_string(),
            Verdict::TooBig { detail } => detail.clone(),
            Verdict::Skip { reason } => reason.clone(),
        }
    }
    /// Colour from the shared palette in `display`, following the same
    /// conventions as the other suites: green pass, magenta crash, red wrong
    /// answer, yellow for "never got far enough to answer".
    fn color(&self) -> &'static str {
        match self {
            Verdict::Pass => display::GREEN,
            Verdict::Ice { .. } => display::MAGENTA,
            Verdict::Clang { .. } => display::PURPLE,
            // Green like a pass: the test was rejected exactly as upstream
            // expects, so there is nothing to look at.
            Verdict::Xfail { .. } => display::GREEN,
            Verdict::Fail { .. } => display::RED,
            Verdict::Optim { .. } => display::BLUE,
            Verdict::Skip { .. } => display::GRAY,
            Verdict::CompileFail { .. }
            | Verdict::LinkFail { .. }
            | Verdict::Timeout
            | Verdict::TooBig { .. } => display::YELLOW,
        }
    }

    /// Order for the report: crashes first, then wrong answers, then the rest.
    fn severity(&self) -> u8 {
        match self {
            Verdict::Ice { .. } => 0,
            Verdict::Clang { .. } => 1,
            Verdict::Fail { .. } => 2,
            Verdict::Optim { .. } => 3,
            Verdict::Timeout => 4,
            Verdict::CompileFail { .. } => 5,
            Verdict::LinkFail { .. } => 6,
            Verdict::TooBig { .. } => 7,
            Verdict::Xfail { .. } => 8,
            Verdict::Pass | Verdict::Skip { .. } => 9,
        }
    }
}

pub struct TortureResult {
    pub name: String,
    pub opt: OptLevel,
    pub verdict: Verdict,
    /// Why this test was skipped, kept so `-run-skipped` can say where the
    /// skip came from: a manifest entry is ours to delete, a
    /// dg-require-effective-target line is upstream's and only tells us this
    /// target is more capable than GCC's gate assumes.
    pub skip_reason: Option<String>,
}

struct Task {
    name: String,
    path: PathBuf,
    /// Submodule root, needed to locate `execute/builtins/lib/main.c`.
    root: PathBuf,
    /// Per-test flags from dg-options / dg-additional-options.
    dg_flags: Vec<String>,
    /// Set under -run-skipped: why this test is normally skipped.
    skip_reason: Option<String>,
    /// Set when the manifest blames clang for this test's failure.
    clang_note: Option<String>,
    /// Upstream expects this test to be rejected (it carries a dg-error).
    expects_error: bool,
    tier: Tier,
    opt: OptLevel,
}

/// Walk a tier directory.  `execute/builtins/lib` holds helper translation
/// units rather than tests, so it is excluded the way GCC's own harness does.
fn discover(root: &Path, tier: Tier) -> Vec<(String, PathBuf)> {
    fn walk(dir: &Path, root: &Path, out: &mut Vec<(String, PathBuf)>) {
        let entries = match std::fs::read_dir(dir) {
            Ok(e) => e,
            Err(_) => return,
        };
        for entry in entries.flatten() {
            let path = entry.path();
            let rel = path.strip_prefix(root).unwrap_or(&path).to_string_lossy().replace('\\', "/");
            if path.is_dir() {
                if rel == "execute/builtins/lib" {
                    continue;
                }
                walk(&path, root, out);
            } else if path.extension().and_then(|e| e.to_str()) == Some("c")
                && !rel.ends_with("-lib.c")
            {
                // `execute/builtins/X-lib.c` is the companion half of X.c, not a
                // test of its own (see builtins.exp upstream).
                out.push((rel, path));
            }
        }
    }
    let mut out = Vec::new();
    walk(&root.join(tier.dir()), root, &mut out);
    out.sort();
    out
}

/// Filter the dg flags down to the ones this clang accepts.
///
/// The suite is full of GCC-only flags. clang rejects some outright
/// (`-fgraphite`, `-fsched2-use-superblocks`) and merely warns about others
/// (`-fno-tree-vrp`), and which is which moves with every clang release, so the
/// set is probed once against an empty translation unit instead of hand-listed.
/// Probing costs one clang run per distinct flag, a couple of seconds total.
fn probe_flags(
    clang: &Path,
    target: Target,
    work_root: &Path,
    candidates: &BTreeSet<String>,
) -> BTreeSet<String> {
    let tmp = unique_tmp_dir(work_root);
    let _ = std::fs::create_dir_all(&tmp);
    let src = tmp.join("probe.c");
    if std::fs::write(&src, "int probe;\n").is_err() {
        remove_tmp_dir(&tmp);
        return BTreeSet::new();
    }
    let mut ok = BTreeSet::new();
    for flag in candidates {
        let mut cmd = Command::new(clang);
        cmd.arg(format!("--target={}", target.triple()))
            .arg("-c")
            .arg("-nostdlib")
            .arg(flag)
            .arg(&src)
            .arg("-o")
            .arg(tmp.join("probe.o"));
        if let Ok((0, _, _)) = run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
            ok.insert(flag.clone());
        }
    }
    remove_tmp_dir(&tmp);
    ok
}

/// dg flags that clang accepts but that must not be honoured anyway: they
/// change the build in ways the harness controls itself, or need a runtime we
/// do not have.
fn flag_is_rejected(flag: &str) -> bool {
    // -m... is target specific and a hard error; -O... would fight the tier's
    // own optimization sweep.
    !flag.starts_with('-')
        || flag.starts_with("-m")
        || (flag.starts_with("-O") && flag.len() <= 3)
        || flag.starts_with("--param")
        || flag.starts_with("-flto")
        || flag == "-fopenmp"
        || flag == "-fsave-optimization-record"
}

/// Extra translation units a test needs beyond its own source.
///
/// `execute/builtins` is a sub-suite with its own protocol, spelled out in
/// upstream `builtins.exp`: each test is linked together with its `-lib.c`
/// companion (a reference implementation of the library function under test,
/// instrumented with `inside_main`) and with `lib/main.c`, which supplies the
/// actual `main()` and calls the test's `main_test()`.  Compiling the files
/// separately, as every other torture test is compiled, leaves `main`
/// undefined and collides the companion's `memcpy` with compiler-rt's.
fn extra_sources(root: &Path, name: &str, path: &Path) -> Vec<PathBuf> {
    if !name.starts_with("execute/builtins/") {
        return Vec::new();
    }
    let mut extra = Vec::new();
    let companion = path.with_file_name(format!(
        "{}-lib.c",
        path.file_stem().unwrap_or_default().to_string_lossy()
    ));
    if companion.is_file() {
        extra.push(companion);
    }
    let main = root.join("execute/builtins/lib/main.c");
    if main.is_file() {
        extra.push(main);
    }
    extra
}

/// Distinguish a compiler crash from clang rejecting the source.  Both fail the
/// test, but only the first is a bug in us rather than in the test's assumptions.
fn is_ice(stderr: &str) -> bool {
    const MARKERS: &[&str] = &[
        "PLEASE submit a bug report",
        "LLVM ERROR",
        "UNREACHABLE executed",
        "Stack dump:",
        "ran out of registers",
        "Segmentation fault",
        "Bad machine code",
        "*** Bad machine code",
    ];
    MARKERS.iter().any(|m| stderr.contains(m))
        || (stderr.contains("Assertion") && stderr.contains("failed"))
}

/// Replace the test's absolute path with its short name: the report already
/// has the name in its first column, and the full submodule path is 70 columns
/// of noise in front of every diagnostic.
fn shorten(detail: String, path: &Path, name: &str) -> String {
    detail.replace(&*path.to_string_lossy(), name)
}

/// Summarize a link failure by the symbols it could not resolve.  Which
/// symbols are missing is the whole diagnosis here (a `__adddf3` means the
/// test needs 64-bit double, a `printf` means it needs a hosted libc), and
/// lld reports one line per symbol, so the first line alone is not enough.
fn link_error_summary(stderr: &str) -> String {
    let mut syms: Vec<&str> = Vec::new();
    for line in stderr.lines() {
        if let Some(rest) = line.split("undefined symbol: ").nth(1) {
            let sym = rest.trim();
            if !sym.is_empty() && !syms.contains(&sym) {
                syms.push(sym);
            }
        }
    }
    if syms.is_empty() {
        return first_error(stderr);
    }
    let shown = syms.len().min(12);
    let mut out = format!("undefined: {}", syms[..shown].join(" "));
    if syms.len() > shown {
        out.push_str(&format!(" (+{} more)", syms.len() - shown));
    }
    out
}

/// If a link failed only on `link_error` / `link_error0..7`, the test is
/// telling us an optimization did not happen rather than that something is
/// missing. Returns the marker names in that case.
fn optimization_markers(stderr: &str) -> Option<String> {
    let mut syms: Vec<&str> = Vec::new();
    for line in stderr.lines() {
        if let Some(rest) = line.split("undefined symbol: ").nth(1) {
            let sym = rest.trim();
            if !sym.is_empty() && !syms.contains(&sym) {
                syms.push(sym);
            }
        }
    }
    let is_marker = |s: &str| {
        s.trim_start_matches('_')
            .strip_prefix("link_error")
            .is_some_and(|rest| rest.chars().all(|c| c.is_ascii_digit()))
    };
    if syms.is_empty() || !syms.iter().all(|s| is_marker(s)) {
        return None;
    }
    Some(syms.join(" "))
}

/// Whatever a crashing compiler managed to print, if anything.
fn crash_hint(stderr: &str) -> String {
    match stderr.lines().find(|l| l.contains("error") || l.contains("Stack dump")) {
        Some(l) => format!(": {}", l.trim().chars().take(90).collect::<String>()),
        None => String::new(),
    }
}

/// First useful line of a diagnostic stream.
///
/// The MachineVerifier prints one `*** Bad machine code: <reason> ***` per
/// violation and only then a "Found N machine code errors" fatal error, so the
/// reason has to be preferred over the first `error:` line or every verifier
/// failure reads the same.
fn first_error(stderr: &str) -> String {
    if let Some(l) = stderr.lines().find(|l| l.contains("Bad machine code:")) {
        let reason = l.split("Bad machine code:").nth(1).unwrap_or(l);
        return format!(
            "bad machine code: {}",
            reason.trim().trim_end_matches('*').trim().chars().take(120).collect::<String>()
        );
    }
    first_error_line(stderr)
}

fn first_error_line(stderr: &str) -> String {
    for line in stderr.lines() {
        if line.contains("error:") || line.contains("LLVM ERROR") {
            return line.trim().chars().take(160).collect();
        }
    }
    stderr
        .lines()
        .find(|l| !l.trim().is_empty())
        .map(|l| l.trim().chars().take(160).collect())
        .unwrap_or_else(|| "error".to_string())
}

/// Assemble `torture/shim/<target>/shim.asm` into the build tree, rebuilding
/// only when stale.  Mirrors how runtime.rs stages the compiler-rt sources.
fn ensure_shim(paths: &Paths, target: Target, clang: &Path) -> Result<PathBuf, String> {
    let src = paths.torture_shim(target);
    if !src.is_file() {
        return Err(format!("shim not found: {}", src.display()));
    }
    let stage = paths.elf_runtime_stage(target);
    std::fs::create_dir_all(&stage).map_err(|e| format!("create {}: {e}", stage.display()))?;
    let obj = stage.join("torture_shim.o");

    let stale = match (std::fs::metadata(&obj).and_then(|m| m.modified()),
                       std::fs::metadata(&src).and_then(|m| m.modified())) {
        (Ok(o), Ok(s)) => s > o,
        _ => true,
    };
    if stale {
        let status = Command::new(clang)
            .arg(format!("--target={}", target.triple()))
            .arg("-c")
            .arg(&src)
            .arg("-o")
            .arg(&obj)
            .status()
            .map_err(|e| format!("assemble shim: {e}"))?;
        if !status.success() {
            return Err(format!("clang failed to assemble {}", src.display()));
        }
    }
    Ok(obj)
}

fn compile_args(
    config: &TortureConfig,
    paths: &Paths,
    opt: OptLevel,
    dg_flags: &[String],
) -> Vec<String> {
    let mut args = vec![
        format!("--target={}", config.target.triple()),
        format!("-{}", opt.clang_flag()),
        "-c".into(),
        "-nostdlib".into(),
        "-w".into(),
        format!("-std={}", config.std),
        // GCC's `dg-add-options stack_size` hands the test the board's stack
        // budget, and 14 execute tests size their local arrays from it. Without
        // it they fall back to a default sized for a hosted machine: 921202-1
        // puts three 2056-element long arrays on the stack, some 24 KB, and
        // simply overruns it. The macro is inert in tests that do not read it.
        format!("-DSTACK_SIZE={STACK_SIZE}"),
        // The torture suite predates C99 and is full of implicit declarations,
        // implicit int, and loose pointer conversions. GCC runs it with
        // -fpermissive for exactly this reason; without the equivalent here,
        // hundreds of tests fail on a language-strictness diagnostic that says
        // nothing about the backend. -w then hides the demoted warnings.
        "-Wno-error=implicit-function-declaration".into(),
        "-Wno-error=implicit-int".into(),
        "-Wno-error=int-conversion".into(),
        "-Wno-error=incompatible-pointer-types".into(),
        "-Wno-error=return-mismatch".into(),
        "-Wno-error=declaration-missing-parameter-type".into(),
        // clang makes `a > b > c` an error by default; GCC only warns.
        "-Wno-error=parentheses".into(),
        "-I".into(),
        paths.torture_include().to_string_lossy().to_string(),
    ];
    // GCC does not run the torture suite freestanding, and the flag is not
    // harmless here: it stops clang from recognising alloca(), turning a
    // feature the Z80 backend implements into an undefined symbol.
    if config.freestanding {
        args.push("-ffreestanding".into());
    }
    if config.verify {
        args.push("-mllvm".into());
        args.push("-verify-machineinstrs".into());
    }
    // Last, so a test's own -std= overrides the runner's default.
    args.extend(dg_flags.iter().cloned());
    args
}

fn run_one(
    task: &Task,
    config: &TortureConfig,
    paths: &Paths,
    clang: &Path,
    rt: Option<&(ElfRuntime, PathBuf)>,
    work_root: &Path,
) -> Verdict {
    match run_one_inner(task, config, paths, clang, rt, work_root) {
        Verdict::Ice { detail } => Verdict::Ice { detail: shorten(detail, &task.path, &task.name) },
        Verdict::CompileFail { detail } => {
            Verdict::CompileFail { detail: shorten(detail, &task.path, &task.name) }
        }
        Verdict::LinkFail { detail } => {
            Verdict::LinkFail { detail: shorten(detail, &task.path, &task.name) }
        }
        Verdict::Optim { detail } => {
            Verdict::Optim { detail: shorten(detail, &task.path, &task.name) }
        }
        Verdict::Clang { detail } => {
            Verdict::Clang { detail: shorten(detail, &task.path, &task.name) }
        }
        other => other,
    }
}

fn run_one_inner(
    task: &Task,
    config: &TortureConfig,
    paths: &Paths,
    clang: &Path,
    rt: Option<&(ElfRuntime, PathBuf)>,
    work_root: &Path,
) -> Verdict {
    let tmp = unique_tmp_dir(work_root);
    let _ = std::fs::create_dir_all(&tmp);

    let mut sources = vec![task.path.clone()];
    if task.tier == Tier::Execute {
        sources.extend(extra_sources(&task.root, &task.name, &task.path));
    }

    let mut objs = Vec::new();
    for (i, src) in sources.iter().enumerate() {
        let obj = tmp.join(format!("t{i}.o"));
        if let Some(v) = compile_one(src, &obj, config, paths, clang, task.opt, &task.dg_flags) {
            remove_tmp_dir(&tmp);
            return v;
        }
        objs.push(obj);
    }

    if task.tier == Tier::Compile {
        remove_tmp_dir(&tmp);
        return Verdict::Pass;
    }
    let rt = rt.expect("execute tier requires the ELF runtime");
    link_and_run(task, config, clang, rt, &tmp, &objs)
}

/// Compile one translation unit. Returns the failing verdict, or None on success.
fn compile_one(
    src: &Path,
    obj: &Path,
    config: &TortureConfig,
    paths: &Paths,
    clang: &Path,
    opt: OptLevel,
    dg_flags: &[String],
) -> Option<Verdict> {
    let mut cmd = Command::new(clang);
    cmd.args(compile_args(config, paths, opt, dg_flags));
    cmd.arg(src).arg("-o").arg(obj);
    match run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
        Err(e) => Some(if e.contains("timeout") {
            Verdict::Ice { detail: format!("compile {e}") }
        } else {
            Verdict::CompileFail { detail: e }
        }),
        Ok((code, _, stderr)) if code < 0 => Some(Verdict::Ice {
            detail: format!("killed by signal {}{}", -code, crash_hint(&stderr)),
        }),
        Ok((code, _, stderr)) if code != 0 || !obj.exists() => Some(if is_ice(&stderr) {
            Verdict::Ice { detail: first_error(&stderr) }
        } else {
            Verdict::CompileFail { detail: first_error(&stderr) }
        }),
        // A MachineVerifier complaint can appear even on a successful exit; it
        // is still a bug.
        Ok((_, _, stderr)) if is_ice(&stderr) => {
            Some(Verdict::Ice { detail: first_error(&stderr) })
        }
        Ok(_) => None,
    }
}

fn link_and_run(
    task: &Task,
    config: &TortureConfig,
    clang: &Path,
    rt: &(ElfRuntime, PathBuf),
    tmp: &Path,
    objs: &[PathBuf],
) -> Verdict {
    let (elf_rt, shim) = rt;
    let elf = tmp.join("t.elf");
    let bin = tmp.join("t.bin");

    let mut link = Command::new(clang.parent().unwrap().join("ld.lld"));
    link.arg("--gc-sections").arg("-T").arg(&elf_rt.linker_script);
    // builtins.exp links its tests with -Wl,--allow-multiple-definition on ELF
    // targets: a builtins test deliberately supplies its own memcpy/strlen,
    // which would otherwise collide with compiler-rt's. Scoped to that
    // sub-suite so a genuine duplicate elsewhere still surfaces as an error.
    if task.name.starts_with("execute/builtins/") {
        link.arg("--allow-multiple-definition");
    }
    link.arg(&elf_rt.crt0_obj);
    for o in objs {
        link.arg(o);
    }
    link.arg(shim);
    // compiler-rt is a library, so give it archive semantics: a member is
    // pulled in only if something still needs it. Several torture tests define
    // their own memcpy/strlen/rintf to check that the compiler either calls or
    // folds them, and linking every builtin object unconditionally made those
    // collide (`duplicate symbol: _rintf`).
    link.arg("--start-lib");
    for o in &elf_rt.builtin_objs {
        link.arg(o);
    }
    link.arg("--end-lib");
    link.arg("-o").arg(&elf);
    match run_cmd_timeout(&mut link, LINK_TIMEOUT) {
        Err(e) => {
            remove_tmp_dir(tmp);
            return Verdict::LinkFail { detail: e };
        }
        Ok((code, _, stderr)) if code != 0 || !elf.exists() => {
            remove_tmp_dir(tmp);
            // "relocation ... out of range: 515170 is not in [-32768, 65535]"
            // means .bss alone overran the 64 KB address space.
            if let Some(markers) = optimization_markers(&stderr) {
                return Verdict::Optim {
                    detail: format!("not optimized away: {markers}"),
                };
            }
            if stderr.contains("out of range") {
                return Verdict::TooBig {
                    detail: "does not fit in the 64 KB address space".into(),
                };
            }
            return Verdict::LinkFail { detail: link_error_summary(&stderr) };
        }
        Ok(_) => {}
    }

    let objcopy = clang.parent().unwrap().join("llvm-objcopy");
    if let Err(e) = emulator::elf_to_bin(&objcopy, &elf, &bin) {
        remove_tmp_dir(tmp);
        return Verdict::LinkFail { detail: e };
    }
    if let Ok(meta) = std::fs::metadata(&bin) {
        if meta.len() > 0x1_0000 {
            remove_tmp_dir(tmp);
            return Verdict::TooBig {
                detail: format!("{} B exceeds the 64 KB address space", meta.len()),
            };
        }
    }

    let nm = clang.parent().unwrap().join("llvm-nm");
    let halt = match emulator::halt_addr_from_elf(&nm, &elf) {
        Some(a) => a,
        None => {
            remove_tmp_dir(tmp);
            return Verdict::LinkFail { detail: "_halt not found in ELF".into() };
        }
    };
    let result_addr = match emulator::symbol_addr_from_elf(&nm, &elf, "_exitcode") {
        Some(a) => a,
        None => {
            remove_tmp_dir(tmp);
            return Verdict::LinkFail { detail: "_exitcode not found in ELF".into() };
        }
    };
    let dump = tmp.join("ram.bin");
    let v = match emulator::run_program(
        &bin, config.target, &halt, result_addr, &dump, config.emu_timeout)
        .map(|r| r.value)
    {
        // A cycle-limit stop is a timeout too: the program never reached
        // _halt within its budget. Left as a LinkFail it would report as a
        // link error, and which of the two fires is a race with the wall
        // clock rather than a property of the test.
        Err(e) if e.contains("timeout") || e.contains("cycle limit") => Verdict::Timeout,
        Err(e) => Verdict::LinkFail { detail: e },
        // Every torture execute test succeeds by leaving 0 in the return
        // register, so there is no per-test expected value to parse.
        Ok(got) => match emulator::check_result(&got, "0000") {
            Ok(()) => Verdict::Pass,
            Err((got_padded, _)) => Verdict::Fail { got: format!("0x{got_padded}") },
        },
    };
    // Keep the working directory on failure so the .elf/.bin can be inspected.
    if v.is_pass() {
        remove_tmp_dir(tmp);
    }
    v
}

pub fn run(paths: &Paths, config: &TortureConfig) -> bool {
    let root = paths.torture_src_dir();
    let root = root.canonicalize().unwrap_or(root);
    if !root.join("execute").is_dir() {
        eprintln!("torture sources not found at {}", root.display());
        eprintln!("initialize the submodule:");
        eprintln!("  git submodule update --init z80-utils/vendor/gcc-torture");
        return false;
    }

    let manifest = match Manifest::load(&paths.torture_manifest()) {
        Ok(m) => m,
        Err(e) => {
            eprintln!("manifest: {e}");
            return false;
        }
    };

    let clang = paths.clang();
    // Scratch goes to the system temp dir, never into the checkout: a failing
    // test keeps its directory for inspection, and thousands of those must not
    // accumulate inside the repository.
    let work_root = std::env::temp_dir().join("z80-torture");
    let _ = std::fs::create_dir_all(&work_root);
    crate::suite::cleanup_old_tmp_dirs(&work_root);

    let needs_runtime = config.tiers.contains(&Tier::Execute);
    let rt = if needs_runtime {
        match (runtime::ensure_elf(paths, config.target, &clang),
               ensure_shim(paths, config.target, &clang)) {
            (Ok(r), Ok(s)) => Some((r, s)),
            (Err(e), _) | (_, Err(e)) => {
                eprintln!("torture runtime: {e}");
                return false;
            }
        }
    } else {
        None
    };

    let mut tasks = Vec::new();
    let mut skipped = Vec::new();
    let mut dg_candidates: BTreeSet<String> = BTreeSet::new();
    for &tier in &config.tiers {
        for (name, path) in discover(&root, tier) {
            if let Some(pat) = &config.pattern {
                if !name.contains(pat.as_str()) {
                    continue;
                }
            }
            // Upstream states per test what the target must provide and which
            // flags the test needs; read it rather than second-guessing it.
            let source = std::fs::read_to_string(&path).unwrap_or_default();
            let dg = torture_data::parse_dg(&source);
            let dg_flags: Vec<String> = dg
                .flags
                .into_iter()
                .chain(torture_data::dot_x_flags(&path))
                .filter(|f| !flag_is_rejected(f))
                .collect();
            dg_candidates.extend(dg_flags.iter().cloned());

            for &opt in &config.opt_levels {
                let reason = manifest
                    .skip_reason(&name, config.target, opt)
                    .map(str::to_string)
                    // dg-skip-if tells the harness not to build the test at
                    // all, so it applies to both tiers. It can narrow to
                    // particular compile options, so it is evaluated per level.
                    .or_else(|| {
                        torture_data::dg_skip_if(&source, opt.flag())
                            .map(|(f, r)| format!("{r} ({f})"))
                    })
                    .or_else(|| {
                        // dg-require-effective-target is about *running* the
                        // test. The compile tier asserts nothing but "clang
                        // accepts this", and 19 of the tests gated this way
                        // compile perfectly well, so it gates execute only.
                        if tier != Tier::Execute {
                            return None;
                        }
                        dg.missing
                            .as_ref()
                            .map(|(f, r)| format!("{r} (dg-require-effective-target {f})"))
                    });
                let clang_note = manifest
                    .clang_note(&name, config.target, opt)
                    .map(str::to_string);
                let expects_error = dg.expects_error;
                let make = |skip_reason: Option<String>| Task {
                    name: name.clone(),
                    path: path.clone(),
                    root: root.clone(),
                    dg_flags: dg_flags.clone(),
                    skip_reason,
                    clang_note: clang_note.clone(),
                    expects_error,
                    tier,
                    opt,
                };
                match reason {
                    Some(reason) if !config.run_skipped => skipped.push(TortureResult {
                        name: name.clone(),
                        opt,
                        verdict: Verdict::Skip { reason },
                        skip_reason: None,
                    }),
                    Some(reason) => tasks.push(make(Some(reason))),
                    None if config.run_skipped => {}
                    None => tasks.push(make(None)),
                }
            }
        }
    }

    let accepted = probe_flags(&clang, config.target, &work_root, &dg_candidates);
    let dropped = dg_candidates.len() - accepted.len();
    for t in &mut tasks {
        t.dg_flags.retain(|f| accepted.contains(f));
    }

    let tiers: Vec<&str> = config.tiers.iter().map(|t| t.dir()).collect();
    let opts: Vec<&str> = config.opt_levels.iter().map(|o| o.clang_flag()).collect();
    println!(
        "torture/{} {} {}  {} tests, {} jobs, -std={}, emu {}s{}",
        tiers.join("+"),
        config.target,
        opts.join(","),
        tasks.len(),
        config.jobs,
        config.std,
        config.emu_timeout,
        if config.run_skipped { "  [-run-skipped]" } else { "" }
    );
    if !dg_candidates.is_empty() {
        println!(
            "  dg-options: {} distinct flags, {} accepted by this clang, {dropped} dropped",
            dg_candidates.len(),
            accepted.len()
        );
    }
    if manifest.is_empty() && !config.run_skipped {
        println!("  (manifest is empty: nothing is skipped yet)");
    }

    let next = AtomicUsize::new(0);
    let done = AtomicUsize::new(0);
    let results: Mutex<Vec<TortureResult>> = Mutex::new(Vec::new());
    let total = tasks.len();
    let start = std::time::Instant::now();

    std::thread::scope(|scope| {
        for _ in 0..config.jobs.max(1) {
            scope.spawn(|| {
                loop {
                    let i = next.fetch_add(1, Ordering::Relaxed);
                    if i >= tasks.len() {
                        break;
                    }
                    let task = &tasks[i];
                    let mut verdict = run_one(task, config, paths, &clang, rt.as_ref(), &work_root);
                    // A dg-error test is supposed to be rejected. Being
                    // rejected is the pass; compiling cleanly is the bug. A
                    // crash is neither, so it stays an ICE.
                    if task.expects_error {
                        verdict = match verdict {
                            Verdict::CompileFail { detail } => Verdict::Xfail { detail },
                            Verdict::Pass => Verdict::Fail {
                                got: "no diagnostic, but dg-error expects one".into(),
                            },
                            other => other,
                        };
                    }
                    if !verdict.is_ok() {
                        if let Some(note) = &task.clang_note {
                            verdict = Verdict::Clang {
                                detail: format!("{note} [{}]", verdict.label()),
                            };
                        }
                    }
                    results.lock().unwrap().push(TortureResult {
                        name: task.name.clone(),
                        opt: task.opt,
                        verdict,
                        skip_reason: task.skip_reason.clone(),
                    });
                    let n = done.fetch_add(1, Ordering::Relaxed) + 1;
                    let tty = display::is_tty();
                    if n == total || n % if tty { 10 } else { 250 } == 0 {
                        // No ETA: it would be a linear extrapolation from the
                        // average, but a single test can sit on the emulator
                        // budget for 30s, so the estimate is noise.
                        let el = start.elapsed().as_secs_f64();
                        let line = format!("  {n}/{total}  {el:.0}s elapsed");
                        if tty {
                            // Repaint in place; erased once the run finishes.
                            print!("\r{}\x1b[K", display::paint(display::DIM, &line));
                            let _ = std::io::stdout().flush();
                        } else {
                            println!("{line}");
                        }
                    }
                }
            });
        }
    });

    if display::is_tty() {
        print!("\r\x1b[K");
        let _ = std::io::stdout().flush();
    }

    let mut results = results.into_inner().unwrap();
    results.extend(skipped);
    report(&mut results, config)
}

fn report(results: &mut [TortureResult], config: &TortureConfig) -> bool {
    results.sort_by(|a, b| {
        a.verdict
            .severity()
            .cmp(&b.verdict.severity())
            .then_with(|| a.name.cmp(&b.name))
            .then_with(|| a.opt.clang_flag().cmp(b.opt.clang_flag()))
    });

    // Counts in severity order, which is the order `results` is now in.
    let mut counts: Vec<(&str, &str, usize)> = Vec::new();
    for r in results.iter() {
        let l = r.verdict.label();
        match counts.iter_mut().find(|(k, _, _)| *k == l) {
            Some((_, _, n)) => *n += 1,
            None => counts.push((l, r.verdict.color(), 1)),
        }
    }
    let summary = || {
        let cells: Vec<String> = counts
            .iter()
            .map(|(k, c, n)| display::paint(&format!("{}{}", display::BOLD, c), &format!("{k} {n}")))
            .collect();
        println!("  {}", cells.join("   "));
    };

    if config.run_skipped {
        let now_passing: Vec<&TortureResult> =
            results.iter().filter(|r| r.verdict.is_pass()).collect();
        if now_passing.is_empty() {
            println!("  every skipped test still fails; the skip list is accurate");
        } else {
            println!("  now passing ({}):", now_passing.len());
            for r in now_passing {
                println!(
                    "    {:<40} {:<3} {}",
                    r.name,
                    r.opt.clang_flag(),
                    r.skip_reason.as_deref().unwrap_or("")
                );
            }
            println!();
            println!(
                "  a manifest entry above is ours to delete; a \
dg-require-effective-target one is upstream's and just means this target is \
more capable than GCC's gate assumes."
            );
        }
        println!();
        summary();
        return true;
    }

    let mut last_label = "";
    for r in results.iter() {
        if r.verdict.is_ok() {
            continue;
        }
        if r.verdict.label() != last_label {
            last_label = r.verdict.label();
            let n = counts.iter().find(|(k, _, _)| *k == last_label).map(|(_, _, n)| *n).unwrap_or(0);
            println!("{}", display::paint(r.verdict.color(), &format!("{last_label} ({n})")));
        }
        println!("  {:<44} {:<3} {}", r.name, r.opt.clang_flag(), r.verdict.detail());
    }

    if let Some(path) = &config.list_failures {
        let mut out = String::new();
        for r in results.iter().filter(|r| !r.verdict.is_ok()) {
            out.push_str(&format!("{}\t{}\t{}\n", r.name, r.opt.clang_flag(), r.verdict.label()));
        }
        match std::fs::write(path, out) {
            Ok(()) => println!("\nfailures written to {}", path.display()),
            Err(e) => eprintln!("cannot write {}: {e}", path.display()),
        }
    }

    println!();
    summary();
    results.iter().all(|r| r.verdict.is_ok())
}
