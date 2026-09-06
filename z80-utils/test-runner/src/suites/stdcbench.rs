//! stdcbench, built with clang and with SDCC and compared on the same terms
//! as the `bench` suite: code size and emulated cycles.
//!
//! Only the c90base module is built. It calls nothing from the C library,
//! which is what makes it usable here at all; c90lib needs malloc, qsort and
//! the string functions, and upstream has not implemented c90float or
//! c90double. `stdcbench/portme.{c,h}` supplies the target hooks, so the
//! mirror under `vendor/stdcbench` stays byte-for-byte upstream.
//!
//! What is reported is cycles, not a stdcbench score. stdcbench scores work
//! done per unit of wall-clock time, and the emulator has no clock for the
//! guest to read, so the harness pins the iteration count instead and
//! measures the run exactly. The benchmark's own result checks still run, and
//! a failure is reported.

use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};

use crate::bench::{elf_text_size, ihx_code_size};
use crate::config::{self, OptLevel, Paths, Target};
use crate::runtime::{self, ElfRuntime};
use owo_colors::OwoColorize;

use crate::report::{self, Align, Table};
use crate::{display, emulator, suite};

const COMPILE_TIMEOUT: u64 = 60;
const LINK_TIMEOUT: u64 = 60;
/// One iteration of c90base costs on the order of 1e8 cycles; several
/// iterations still sit far inside this.
const EMU_CYCLES: u64 = 40_000_000_000;

pub struct StdcbenchConfig {
    pub target: Target,
    pub opt: OptLevel,
    /// How many times c90base repeats its work loop. Emulation is exact, so
    /// one iteration already measures the whole benchmark without noise;
    /// more only costs time.
    pub iterations: u32,
}

struct Measurement {
    size: u32,
    cycles: u64,
    /// stdcbench's own result checks failed somewhere in the run.
    failed_check: bool,
}

pub fn run(paths: &Paths, config: &StdcbenchConfig) -> bool {
    let src_dir = paths.stdcbench_src_dir();
    let harness_dir = paths.stdcbench_harness_dir();

    if !src_dir.join("stdcbench.c").is_file() {
        eprintln!(
            "stdcbench sources not found under {}.\n\
             Initialize the submodule with:\n  \
             git submodule update --init z80-utils/vendor/stdcbench",
            src_dir.display()
        );
        return false;
    }

    let mut sources = match collect_sources(&src_dir) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("{e}");
            return false;
        }
    };
    sources.push(harness_dir.join("portme.c"));

    let clang = paths.clang();
    let elf_rt = match runtime::ensure_elf(paths, config.target, &clang) {
        Ok(rt) => rt,
        Err(e) => {
            eprintln!("ELF runtime stage failed: {e}");
            return false;
        }
    };

    report::heading(&format!(
        "{} stdcbench c90base  {}, {} iteration{}",
        config.target.triple().to_uppercase(),
        config.opt,
        config.iterations,
        if config.iterations == 1 { "" } else { "s" }
    ));

    let work_root = std::env::temp_dir().join("z80-stdcbench");
    suite::cleanup_old_tmp_dirs(&work_root);
    let _ = std::fs::create_dir_all(&work_root);
    let tmp = suite::unique_tmp_dir(&work_root);
    if let Err(e) = std::fs::create_dir_all(&tmp) {
        eprintln!("create {}: {e}", tmp.display());
        return false;
    }

    let clang_m = build_clang(&clang, &sources, &src_dir, &harness_dir, &tmp, config, &elf_rt);
    let sdcc_m = build_sdcc(paths, &sources, &src_dir, &harness_dir, &tmp, config);

    suite::remove_tmp_dir(&tmp);

    report(clang_m.as_ref(), sdcc_m.as_ref())
}

/// Every `.c` in the mirror. The modules the harness disables compile to
/// nothing, so there is no list here to fall out of step with upstream.
fn collect_sources(src_dir: &Path) -> Result<Vec<PathBuf>, String> {
    let mut sources: Vec<PathBuf> = std::fs::read_dir(src_dir)
        .map_err(|e| format!("read_dir {}: {e}", src_dir.display()))?
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.extension().and_then(|s| s.to_str()) == Some("c"))
        .collect();
    if sources.is_empty() {
        return Err(format!("no .c sources under {}", src_dir.display()));
    }
    sources.sort();
    Ok(sources)
}

fn build_clang(
    clang: &Path,
    sources: &[PathBuf],
    src_dir: &Path,
    harness_dir: &Path,
    tmp: &Path,
    config: &StdcbenchConfig,
    elf_rt: &ElfRuntime,
) -> Option<Measurement> {
    let mut objs = Vec::new();
    for src in sources {
        let obj = tmp.join(format!("clang_{}.o", stem(src)));
        let mut cmd = Command::new(clang);
        cmd.arg(format!("--target={}", config.target.triple()));
        cmd.arg(format!("-{}", config.opt.clang_flag()));
        cmd.args(["-c", "-nostdlib", "-ffreestanding", "-w", "-std=c99"]);
        cmd.arg(format!("-DSTDCBENCH_ITERATIONS={}", config.iterations));
        cmd.arg("-I").arg(harness_dir);
        cmd.arg("-I").arg(src_dir);
        cmd.arg(src).arg("-o").arg(&obj);
        match suite::run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
            Ok((0, _, _)) => {}
            Ok((_, _, stderr)) => {
                eprintln!("clang: {} failed: {}", stem(src), suite::extract_error(&stderr));
                return None;
            }
            Err(e) => {
                eprintln!("clang: {} failed: {e}", stem(src));
                return None;
            }
        }
        objs.push(obj);
    }

    let elf = tmp.join("clang.elf");
    let bin = tmp.join("clang.bin");
    let tools = clang.parent().unwrap();

    let mut link = Command::new(tools.join("ld.lld"));
    link.arg("--gc-sections");
    link.arg("-T").arg(&elf_rt.linker_script);
    link.arg(&elf_rt.crt0_obj);
    for o in &objs {
        link.arg(o);
    }
    link.arg("--start-lib");
    for o in &elf_rt.builtin_objs {
        link.arg(o);
    }
    link.arg("--end-lib");
    link.arg("-o").arg(&elf);
    match suite::run_cmd_timeout(&mut link, LINK_TIMEOUT) {
        Ok((0, _, _)) if elf.exists() => {}
        Ok((_, _, stderr)) => {
            eprintln!("clang: link failed: {}", suite::extract_error(&stderr));
            return None;
        }
        Err(e) => {
            eprintln!("clang: link failed: {e}");
            return None;
        }
    }

    if let Err(e) = emulator::elf_to_bin(&tools.join("llvm-objcopy"), &elf, &bin) {
        eprintln!("clang: {e}");
        return None;
    }

    let nm = tools.join("llvm-nm");
    let halt = emulator::halt_addr_from_elf(&nm, &elf)?;
    let berror = emulator::symbol_addr_from_elf(&nm, &elf, "_berror")?;
    let size = elf_text_size(&tools.join("llvm-size"), &elf)
        .unwrap_or_else(|| std::fs::metadata(&bin).map(|m| m.len() as u32).unwrap_or(0));

    measure("clang", &bin, config.target, &halt, berror, size, tmp)
}

fn build_sdcc(
    paths: &Paths,
    sources: &[PathBuf],
    src_dir: &Path,
    harness_dir: &Path,
    tmp: &Path,
    config: &StdcbenchConfig,
) -> Option<Measurement> {
    let crt0 = match runtime::ensure_sdcc_crt0(paths, config.target) {
        Ok(p) => p,
        Err(e) => {
            eprintln!("sdcc: harness crt0 unavailable: {e}");
            return None;
        }
    };

    let sdcc_opt = match config.opt {
        OptLevel::Os | OptLevel::Oz => "--opt-code-size",
        OptLevel::O2 | OptLevel::O3 => "--opt-code-speed",
        _ => "",
    };

    let mut rels = Vec::new();
    for src in sources {
        let stem = stem(src);
        let asm = tmp.join(format!("sdcc_{stem}.asm"));
        let rel = tmp.join(format!("sdcc_{stem}.rel"));

        let mut cmd = Command::new("sdcc");
        cmd.arg(config.target.sdcc_flag());
        cmd.args(["--std-c11", "-S"]);
        if !sdcc_opt.is_empty() {
            cmd.arg(sdcc_opt);
        }
        cmd.arg(format!("-DSTDCBENCH_ITERATIONS={}", config.iterations));
        cmd.arg("-I").arg(harness_dir);
        cmd.arg("-I").arg(src_dir);
        cmd.arg(src).arg("-o").arg(&asm);
        match suite::run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
            Ok((0, _, _)) => {}
            Ok((_, _, stderr)) => {
                eprintln!("sdcc: {stem} failed: {}", suite::extract_error(&stderr));
                return None;
            }
            Err(e) => {
                eprintln!("sdcc: {stem} failed: {e}");
                return None;
            }
        }

        let ok = Command::new(config.target.assembler())
            .args(["-g", "-o"])
            .arg(&rel)
            .arg(&asm)
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .status()
            .map(|s| s.success())
            .unwrap_or(false);
        if !ok {
            eprintln!("sdcc: assembling {stem} failed");
            return None;
        }
        rels.push(rel);
    }

    let base = tmp.join("sdcc");
    let mut link = Command::new(config.target.linker());
    link.args(["-m", "-i"]);
    link.arg(&base);
    link.arg(&crt0);
    for r in &rels {
        link.arg(r);
    }
    if let Some(lib) = config::find_sdcc_lib(config.target) {
        if let Some(dir) = lib.parent() {
            link.arg("-k").arg(dir);
            link.arg("-l").arg(config.target.triple());
        }
    }
    link.stdout(Stdio::null()).stderr(Stdio::null());
    if link.status().map(|s| !s.success()).unwrap_or(true) {
        eprintln!("sdcc: link failed");
        return None;
    }

    let ihx = tmp.join("sdcc.ihx");
    let bin = tmp.join("sdcc.bin");
    if let Err(e) = emulator::makebin(&ihx, &bin) {
        eprintln!("sdcc: {e}");
        return None;
    }

    let map = tmp.join("sdcc.map");
    let halt = emulator::halt_addr_from_map(&map)?;
    let berror = emulator::symbol_addr_from_map(&map, "_berror")?;
    let size = ihx_code_size(&ihx);

    measure("sdcc", &bin, config.target, &halt, berror, size, tmp)
}

fn measure(
    who: &str,
    bin: &Path,
    target: Target,
    halt: &str,
    berror_addr: u32,
    size: u32,
    tmp: &Path,
) -> Option<Measurement> {
    let dump = tmp.join(format!("{who}.ram"));
    match emulator::run_program(bin, target, halt, berror_addr, &dump, EMU_CYCLES) {
        Ok(run) => {
            // run_program reads a 16-bit word; berror is the low byte of it.
            let failed_check = !run.value.ends_with("00");
            Some(Measurement { size, cycles: run.cycles, failed_check })
        }
        Err(e) => {
            eprintln!("{who}: {e}");
            None
        }
    }
}

fn report(clang: Option<&Measurement>, sdcc: Option<&Measurement>) -> bool {
    let mut table = Table::new(
        &["Compiler", "Size (B)", "Cycles", "Checks"],
        &[Align::Left, Align::Right, Align::Right, Align::Left],
    );

    let mut ok = true;
    for (name, m) in [("clang", clang), ("sdcc", sdcc)] {
        match m {
            Some(m) => {
                let checks = if m.failed_check {
                    ok = false;
                    format!("{}", "FAILED".style(display::bad()))
                } else {
                    format!("{}", "ok".style(display::ok()))
                };
                table.row([
                    name.to_string(),
                    m.size.to_string(),
                    report::thousands(m.cycles),
                    checks,
                ]);
            }
            None => {
                ok = false;
                table.row([
                    name.to_string(),
                    format!("{}", "did not build".style(display::bad())),
                    String::new(),
                    String::new(),
                ]);
            }
        }
    }
    table.print();

    let built: Vec<(&str, &Measurement)> = [("clang", clang), ("sdcc", sdcc)]
        .into_iter()
        .filter_map(|(name, m)| m.map(|m| (name, m)))
        .collect();
    if built.len() >= 2 {
        crate::say!();
        let sizes: Vec<(&str, u64)> = built.iter().map(|(n, m)| (*n, m.size as u64)).collect();
        let cycles: Vec<(&str, u64)> = built.iter().map(|(n, m)| (*n, m.cycles)).collect();
        crate::say!("  {}", compare("Size", &sizes));
        crate::say!("  {}", compare("Speed", &cycles));
    }

    crate::say!();
    report::outcome(ok, "stdcbench OK", "stdcbench FAILED");
    ok
}

/// Who won and by how much over the next best, across however many
/// toolchains built. Lower is better for both size and cycles, so one
/// comparison serves both.
fn compare(what: &str, entries: &[(&str, u64)]) -> String {
    let mut ranked: Vec<&(&str, u64)> = entries.iter().collect();
    ranked.sort_by_key(|(_, v)| *v);

    let [best, next, ..] = ranked.as_slice() else {
        return format!("{what}: nothing to compare");
    };
    if best.1 == next.1 {
        return format!("{what}: tie");
    }

    let pct = (next.1 - best.1) as f64 / next.1 as f64 * 100.0;
    let adjective = if what == "Size" { "smaller" } else { "faster" };
    format!(
        "{what}: {} by {pct:.1}% over {} ({adjective})",
        best.0.style(display::compiler(best.0).bold()),
        next.0.style(display::compiler(next.0))
    )
}

fn stem(p: &Path) -> String {
    p.file_stem().unwrap_or_default().to_string_lossy().to_string()
}
