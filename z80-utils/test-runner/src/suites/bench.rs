use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::{Arc, Mutex};
use std::thread;

use crate::config::{self, OptLevel, Paths, Target};
use crate::display;
use crate::emulator;
use owo_colors::OwoColorize;

use crate::report::{self, Align, Table};
use crate::runtime::{self, ElfRuntime};
use crate::suite;

const COMPILE_TIMEOUT: u64 = 30;

pub struct BenchConfig {
    pub target: Target,
    pub opt: OptLevel,
    pub pattern: Option<String>,
}

#[derive(Clone, Debug)]
struct BenchResult {
    name: String,
    opt: OptLevel,
    clang: Option<CompilerResult>,
    sdcc: Option<CompilerResult>,
    error: Option<String>,
}

#[derive(Clone, Debug)]
struct CompilerResult {
    size: u32,
    tstates: u64,
    #[allow(dead_code)]
    reg_value: String,
    correct: bool,
}

pub fn run(paths: &Paths, config: &BenchConfig) {
    let bench_dir = paths.project_dir.join("benchmark");
    let clang = paths.clang();

    // Stage crt0 + ELF compiler-rt builtins so the per-bench link can
    // resolve _halt and the runtime calls (ravn/llvm-z80#106; mirrors clang.rs).
    let elf_rt = match runtime::ensure_elf(paths, config.target, &clang) {
        Ok(rt) => Arc::new(rt),
        Err(e) => {
            eprintln!("ELF runtime stage failed: {e}");
            return;
        }
    };

    // Discover benchmarks
    let mut bench_files: Vec<PathBuf> = std::fs::read_dir(&bench_dir)
        .into_iter()
        .flatten()
        .filter_map(|e| e.ok())
        .map(|e| e.path())
        .filter(|p| {
            p.file_name()
                .and_then(|n| n.to_str())
                .is_some_and(|n| n.starts_with("bench_") && n.ends_with(".c"))
        })
        .collect();
    bench_files.sort();

    if let Some(ref pat) = config.pattern {
        bench_files.retain(|p| {
            p.file_stem()
                .and_then(|n| n.to_str())
                .is_some_and(|n| n.contains(pat.as_str()))
        });
    }

    let sdcc_lib = config::find_sdcc_lib(config.target);

    // Print header
    let target_upper = config.target.triple().to_uppercase();
    crate::say!("{target_upper} Compiler Benchmark: Clang vs SDCC");
    crate::say!("======================================");
    crate::say!("Target:   {}", config.target);
    crate::say!("Build:    {}", paths.build_dir.display());
    print!("SDCC:     ");
    if let Ok(output) = Command::new("sdcc").arg("--version").output() {
        let ver = String::from_utf8_lossy(&output.stdout);
        let first = ver.lines().next().unwrap_or("unknown");
        crate::say!("{}", first.strip_prefix("SDCC : ").unwrap_or(first));
    } else {
        crate::say!("not found");
    }
    crate::say!("Opt:      {}", config.opt);
    crate::say!();

    // Run all benchmarks in parallel
    let total = bench_files.len();
    let results: Vec<BenchResult> = {
        let results = Arc::new(Mutex::new(Vec::new()));
        let progress = Arc::new(report::Line::new(total as u64, "benchmarks"));

        let handles: Vec<_> = bench_files
            .into_iter()
            .map(|bench_file| {
                let clang = clang.clone();
                let paths = paths.clone();
                let target = config.target;
                let opt = config.opt;
                let sdcc_lib = sdcc_lib.clone();
                let results = Arc::clone(&results);
                let progress = Arc::clone(&progress);
                let elf_rt = Arc::clone(&elf_rt);

                thread::spawn(move || {
                    let r = run_single_bench(
                        &bench_file, &clang, &paths, target, opt, sdcc_lib.as_ref(),
                        &elf_rt,
                    );
                    results.lock().unwrap().push(r);
                    progress.inc();
                })
            })
            .collect();

        for h in handles {
            let _ = h.join();
        }

        Arc::into_inner(progress).expect("threads joined").clear();

        let mut v = Arc::try_unwrap(results).unwrap().into_inner().unwrap();
        v.sort_by(|a, b| a.name.cmp(&b.name));
        v
    };

    // Print table
    print_table(&results, config);
}

fn run_single_bench(
    bench_file: &Path,
    clang: &Path,
    paths: &Paths,
    target: Target,
    opt: OptLevel,
    sdcc_lib: Option<&PathBuf>,
    elf_rt: &Arc<ElfRuntime>,
) -> BenchResult {
    let name = bench_file
        .file_stem()
        .unwrap()
        .to_string_lossy()
        .to_string();
    let source = std::fs::read_to_string(bench_file).unwrap_or_default();
    let expected = emulator::parse_expected(&source);

    let bench_dir = bench_file.parent().unwrap();
    let tmp_dir = suite::unique_tmp_dir(bench_dir);
    let _ = std::fs::create_dir_all(&tmp_dir);

    // Compile both in parallel
    let clang_result = {
        let clang = clang.to_path_buf();
        let tmp = tmp_dir.clone();
        let name = name.clone();
        let expected = expected.clone();
        let target_copy = target;
        let bench_file = bench_file.to_path_buf();
        let elf_rt = Arc::clone(elf_rt);

        thread::spawn(move || {
            compile_and_measure_clang(
                &clang, &bench_file, &tmp, &name, target_copy, opt, &expected, &elf_rt,
            )
        })
    };

    let sdcc_result = {
        let tmp = tmp_dir.clone();
        let name = name.clone();
        let expected = expected.clone();
        // The harness crt0 records main's return value at _exitcode, which is
        // how the run reports its result.
        let crt0 = match runtime::ensure_sdcc_crt0(paths, target) {
            Ok(p) => Some(p),
            // Leave the SDCC side unmeasured rather than handing the linker an
            // empty path and reporting a confusing link failure.
            Err(_) => None,
        };
        let sdcc_lib = sdcc_lib.cloned();
        let bench_file = bench_file.to_path_buf();

        thread::spawn(move || {
            compile_and_measure_sdcc(
                &bench_file, &tmp, &name, target, opt, &expected, crt0.as_ref()?,
                sdcc_lib.as_ref(),
            )
        })
    };

    let clang_r = clang_result.join().ok().flatten();
    let sdcc_r = sdcc_result.join().ok().flatten();

    suite::remove_tmp_dir(&tmp_dir);

    let error = if clang_r.is_none() || sdcc_r.is_none() {
        let mut errs = Vec::new();
        if clang_r.is_none() { errs.push("Clang: COMPILE_ERROR"); }
        if sdcc_r.is_none() { errs.push("SDCC: COMPILE_ERROR"); }
        Some(errs.join(", "))
    } else {
        None
    };

    BenchResult { name, opt, clang: clang_r, sdcc: sdcc_r, error }
}

fn compile_and_measure_clang(
    clang: &Path,
    src: &Path,
    tmp_dir: &Path,
    name: &str,
    target: Target,
    opt: OptLevel,
    expected: &str,
    elf_rt: &ElfRuntime,
) -> Option<CompilerResult> {
    let tag = format!("{name}_clang_{opt}");
    let test_obj = tmp_dir.join(format!("{tag}.o"));
    let elf = tmp_dir.join(format!("{tag}.elf"));
    let bin = tmp_dir.join(format!("{tag}.bin"));

    // Compile to object only — link below injects crt0 + builtins +
    // linker script explicitly so _halt resolves and ___mulhi3 / etc.
    // are linked in (ravn/llvm-z80#106).
    let mut cmd = Command::new(clang);
    cmd.arg(format!("--target={}", target.triple()));
    cmd.arg(format!("-{}", opt.clang_flag()));
    cmd.arg("-c");
    cmd.arg("-nostdlib");
    cmd.arg("-ffreestanding");
    cmd.arg(src);
    cmd.arg("-o");
    cmd.arg(&test_obj);

    match suite::run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
        Err(_) => return None,
        Ok((code, _, _)) if code != 0 => return None,
        _ => {}
    }

    let lld = clang.parent().unwrap().join("ld.lld");
    let mut link = Command::new(lld);
    link.arg("--gc-sections");
    link.arg("-T");
    link.arg(&elf_rt.linker_script);
    link.arg(&elf_rt.crt0_obj);
    link.arg(&test_obj);
    for obj in &elf_rt.builtin_objs {
        link.arg(obj);
    }
    link.arg("-o");
    link.arg(&elf);

    match suite::run_cmd_timeout(&mut link, COMPILE_TIMEOUT) {
        Err(_) => return None,
        Ok((code, _, _)) if code != 0 => return None,
        _ => {}
    }
    if !elf.exists() { return None; }

    // ELF → flat binary
    let objcopy = clang.parent().unwrap().join("llvm-objcopy");
    if emulator::elf_to_bin(&objcopy, &elf, &bin).is_err() {
        return None;
    }

    // Code size from ELF (text+data sections, includes crt0 and linked runtime)
    let llvm_tools = clang.parent().unwrap();
    let size = elf_text_size(&llvm_tools.join("llvm-size"), &elf)
        .unwrap_or_else(|| std::fs::metadata(&bin).map(|m| m.len() as u32).unwrap_or(0));

    // Halt address from ELF _halt symbol
    let halt_addr = emulator::halt_addr_from_elf(&llvm_tools.join("llvm-nm"), &elf)?;

    // One run yields both the cycle count and the result value.
    let result_addr = emulator::symbol_addr_from_elf(&llvm_tools.join("llvm-nm"), &elf, "_exitcode")?;
    let dump = bin.with_extension("ram");
    let run = emulator::run_program(&bin, target, &halt_addr, result_addr, &dump,
                                    target.emu_cycles()).ok();
    let tstates = run.as_ref().map(|r| r.cycles).unwrap_or(0);
    let reg_value = run.map(|r| r.value).unwrap_or_default();
    let correct = emulator::check_result(&reg_value, expected).is_ok();

    Some(CompilerResult { size, tstates, reg_value, correct })
}

fn compile_and_measure_sdcc(
    src: &Path,
    tmp_dir: &Path,
    name: &str,
    target: Target,
    opt: OptLevel,
    expected: &str,
    crt0: &Path,
    sdcc_lib: Option<&PathBuf>,
) -> Option<CompilerResult> {
    let tag = format!("{name}_sdcc_{opt}");
    let asm_file = tmp_dir.join(format!("{tag}.asm"));
    let rel_file = tmp_dir.join(format!("{tag}.rel"));
    let base = tmp_dir.join(&tag);
    let ihx = tmp_dir.join(format!("{tag}.ihx"));
    let bin = tmp_dir.join(format!("{tag}.bin"));

    // SDCC optimization flags
    let sdcc_opt = match opt {
        OptLevel::Os | OptLevel::Oz => "--opt-code-size",
        OptLevel::O2 | OptLevel::O3 => "--opt-code-speed",
        _ => "",
    };

    // Compile to asm
    let mut cmd = Command::new("sdcc");
    cmd.arg(target.sdcc_flag());
    cmd.args(["--std-c11", "-S"]);
    if !sdcc_opt.is_empty() {
        cmd.arg(sdcc_opt);
    }
    cmd.arg(src);
    cmd.arg("-o");
    cmd.arg(&asm_file);

    match suite::run_cmd_timeout(&mut cmd, COMPILE_TIMEOUT) {
        Err(_) => return None,
        Ok((code, _, _)) if code != 0 => return None,
        _ => {}
    }

    // Assemble
    if Command::new(target.assembler())
        .args(["-g", "-o"])
        .arg(&rel_file)
        .arg(&asm_file)
        .stdout(std::process::Stdio::null())
        .stderr(std::process::Stdio::null())
        .status()
        .map(|s| !s.success())
        .unwrap_or(true)
    {
        return None;
    }

    // Link (with -m for map file to locate _halt symbol)
    let mut cmd = Command::new(target.linker());
    cmd.args(["-m", "-i"]);
    cmd.arg(&base);
    cmd.arg(crt0);
    cmd.arg(&rel_file);
    if let Some(lib) = sdcc_lib {
        if let Some(dir) = lib.parent() {
            cmd.arg("-k");
            cmd.arg(dir);
            cmd.arg("-l");
            cmd.arg(target.triple());
        }
    }
    cmd.stdout(std::process::Stdio::null());
    cmd.stderr(std::process::Stdio::null());

    if cmd.status().map(|s| !s.success()).unwrap_or(true) {
        return None;
    }

    let map_file = tmp_dir.join(format!("{tag}.map"));
    let halt_addr = emulator::halt_addr_from_map(&map_file)?;

    measure_ihx(&ihx, &bin, target, expected, &halt_addr, &map_file)
}

fn measure_ihx(
    ihx: &Path,
    bin: &Path,
    target: Target,
    expected: &str,
    halt_addr: &str,
    map_file: &Path,
) -> Option<CompilerResult> {
    // Code size from IHX data records (actual code+data bytes, not padded binary)
    let size = ihx_code_size(ihx);

    // makebin: IHX → flat binary
    if emulator::makebin(ihx, bin).is_err() {
        return None;
    }

    // One run yields both the cycle count and the result value.
    let result_addr = emulator::symbol_addr_from_map(map_file, "_exitcode")?;
    let dump = bin.with_extension("ram");
    let run = emulator::run_program(bin, target, halt_addr, result_addr, &dump,
                                    target.emu_cycles()).ok();
    let tstates = run.as_ref().map(|r| r.cycles).unwrap_or(0);
    let reg_value = run.map(|r| r.value).unwrap_or_default();
    let correct = emulator::check_result(&reg_value, expected).is_ok();

    Some(CompilerResult { size, tstates, reg_value, correct })
}

/// Extract total loaded section sizes from an ELF using llvm-size.
pub(crate) fn elf_text_size(llvm_size: &Path, elf: &Path) -> Option<u32> {
    let output = Command::new(llvm_size)
        .arg(elf)
        .output()
        .ok()?;
    if !output.status.success() { return None; }
    // llvm-size output (Berkeley format):
    //    text    data     bss     dec     hex filename
    //    1234      56       0    1290     50a file.elf
    let stdout = String::from_utf8_lossy(&output.stdout);
    let line = stdout.lines().nth(1)?;
    let cols: Vec<&str> = line.split_whitespace().collect();
    if cols.len() < 3 { return None; }
    let text: u32 = cols[0].parse().ok()?;
    let data: u32 = cols[1].parse().ok()?;
    Some(text + data)
}

pub(crate) fn ihx_code_size(ihx: &Path) -> u32 {
    let content = std::fs::read_to_string(ihx).unwrap_or_default();
    let mut total = 0u32;
    for line in content.lines() {
        if line.starts_with(':') && line.len() >= 11 {
            let len = u32::from_str_radix(&line[1..3], 16).unwrap_or(0);
            let record_type = u32::from_str_radix(&line[7..9], 16).unwrap_or(255);
            if record_type == 0 {
                total += len;
            }
        }
    }
    total
}


fn print_table(results: &[BenchResult], _config: &BenchConfig) {
    let mut table = Table::new(
        &["Benchmark", "Opt", "Clang (B)", "SDCC (B)", "Clang (T)", "SDCC (T)", "Winner"],
        &[
            Align::Left,
            Align::Left,
            Align::Right,
            Align::Right,
            Align::Right,
            Align::Right,
            Align::Left,
        ],
    );

    let mut total = 0u32;
    let mut errors = 0u32;
    let mut clang_size_wins = 0u32;
    let mut sdcc_size_wins = 0u32;
    let mut clang_speed_wins = 0u32;
    let mut sdcc_speed_wins = 0u32;

    for r in results {
        total += 1;

        let (Some(clang), Some(sdcc)) = (r.clang.as_ref(), r.sdcc.as_ref()) else {
            errors += 1;
            let why = r.error.clone().unwrap_or_else(|| "did not build".into());
            table.row([
                r.name.clone(),
                r.opt.to_string(),
                String::new(),
                String::new(),
                String::new(),
                String::new(),
                format!("{}", format_args!("ERROR ({why})").style(display::bad())),
            ]);
            continue;
        };

        let size_winner = winner(clang.size, sdcc.size);
        let speed_winner = winner(clang.tstates, sdcc.tstates);

        match &size_winner {
            Winner::A => clang_size_wins += 1,
            Winner::B => sdcc_size_wins += 1,
            Winner::Tie => {}
        }
        match &speed_winner {
            Winner::A => clang_speed_wins += 1,
            Winner::B => sdcc_speed_wins += 1,
            Winner::Tie => {}
        }

        let name_of = |w: &Winner| match w {
            Winner::A => "Clang",
            Winner::B => "SDCC",
            Winner::Tie => "Tie",
        };
        let mut verdict = match (&size_winner, &speed_winner) {
            (Winner::A, Winner::A) => format!("{}", "Clang".style(display::compiler("clang"))),
            (Winner::B, Winner::B) => format!("{}", "SDCC".style(display::compiler("sdcc"))),
            (Winner::Tie, Winner::Tie) => "Tie".to_string(),
            (sz, sp) => format!("Size:{} Spd:{}", name_of(sz), name_of(sp)),
        };
        // A wrong answer matters more than who won, so it rides along on the
        // same line rather than in a column readers have to look for.
        for (bad_one, label) in [(!clang.correct, "!Clang"), (!sdcc.correct, "!SDCC")] {
            if bad_one {
                verdict.push_str(&format!(" {}", label.style(display::bad())));
            }
        }

        table.row([
            r.name.clone(),
            r.opt.to_string(),
            clang.size.to_string(),
            sdcc.size.to_string(),
            report::thousands(clang.tstates),
            report::thousands(sdcc.tstates),
            verdict,
        ]);
    }
    table.print();

    let size_tie = total - errors - clang_size_wins - sdcc_size_wins;
    let speed_tie = total - errors - clang_speed_wins - sdcc_speed_wins;
    crate::say!();
    report::fields(&[
        ("Comparisons", format!("{total} ({errors} errors)")),
        ("Code size", format!("Clang={clang_size_wins}  SDCC={sdcc_size_wins}  Tie={size_tie}")),
        ("Speed", format!("Clang={clang_speed_wins}  SDCC={sdcc_speed_wins}  Tie={speed_tie}")),
    ]);
}

pub(crate) enum Winner {
    A,
    B,
    Tie,
}

pub(crate) fn winner<T: PartialOrd>(a: T, b: T) -> Winner {
    if a < b { Winner::A }
    else if a > b { Winner::B }
    else { Winner::Tie }
}
