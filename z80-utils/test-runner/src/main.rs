mod config;
mod display;
mod harness;
mod report;
mod run_all;
mod suites;

// The physical layout groups the modules; the logical paths stay flat, so a
// suite still refers to `crate::emulator` rather than threading the directory
// name through every call site.
pub(crate) use harness::{emulator, runtime, suite};
pub(crate) use suites::{bench, clang, lit, llc, sdcc, stdcbench, torture, torture_data, utils};


use std::process::ExitCode;

use config::{Paths, Target};
use suite::OnResult;

fn main() -> ExitCode {
    // Clean up leftover tmp directories from previous (possibly interrupted) runs.
    let paths = config::Paths::resolve();
    suite::cleanup_old_tmp_dirs(&paths.clang_test_dir());
    suite::cleanup_old_tmp_dirs(&paths.sdcc_test_dir());
    suite::cleanup_old_tmp_dirs(&paths.llc_test_dir());
    suite::cleanup_old_tmp_dirs(&paths.project_dir.join("benchmark"));

    let args: Vec<String> = std::env::args().skip(1).collect();

    // No arguments prints usage rather than launching a long run: the
    // suites take minutes, and starting one by accident is worse than a
    // reminder of what the commands are.
    if args.is_empty() {
        print_help();
        return ExitCode::FAILURE;
    }

    match args[0].as_str() {
        "test" => cmd_run_all(&args[1..]),
        "full" => cmd_full(&args[1..]),
        "bench" => cmd_bench(&args[1..]),
        "clang" => cmd_clang(&args[1..]),
        "sdcc" => cmd_sdcc(&args[1..]),
        "stdcbench" => cmd_stdcbench(&args[1..]),
        "torture" => cmd_torture(&args[1..]),
        "lit" => cmd_lit(&args[1..]),
        "llc" => cmd_llc(&args[1..]),
        "utils" => cmd_utils(&args[1..]),
        "help" | "--help" | "-h" => {
            print_help();
            ExitCode::SUCCESS
        }
        other => {
            eprintln!("unknown command: {other}");
            print_help();
            ExitCode::FAILURE
        }
    }
}

/// Create a callback that prints each test result immediately to stdout.
fn print_callback() -> OnResult {
    Box::new(|result, reg_name| {
        display::print_test_result(&result.outcome, &result.tag, reg_name);
    })
}

fn print_help() {
    eprintln!(
        "\
Usage: z80-test-runner <command> [options]

Commands:
  test       Run the end-to-end suites in parallel
  full       Every suite at every opt level, the lit tests, and torture at
             O1/O2/Os on both targets
  bench      Run Clang vs SDCC benchmark comparison
  clang      Run Clang C test suite
  sdcc       Run SDCC compatibility test suite
  stdcbench  Run stdcbench c90base, Clang vs SDCC
  torture    Run the GCC C torture suite (also run by `full`)
  lit        Run this backend's LLVM lit tests (also run by `full`)
  llc        Run LLC (LLVM IR) test suite
  utils      Run elf2rel/rel2elf roundtrip and crosslink tests
  help       Show this help

Common options:
  -target <z80|sm83>   Target architecture (default: z80)
  -opt <O0|O1|...|all> Optimization level

test options:
  -full      Run every opt level. The `full` command does this and then runs
             the torture suite on both targets.
  -opt <LVL> Run only the specified opt level

The suites with options of their own describe them on request:
  z80-test-runner clang help
  z80-test-runner torture help

Environment:
  BUILD_DIR            Build directory (default: ../build)"
    );
}

fn print_clang_help() {
    eprintln!(
        "\
Usage: z80-test-runner clang [options] [name-filter]

  -target <z80|sm83>   Target architecture (default: z80)
  -opt <O0|O1|...|all> Optimization level (default: all)
  -fast-math           Enable -ffast-math
  -omit-frame-pointer  Enable -fomit-frame-pointer
  -static-stack        Enable +static-stack (BSS locals)
  -freestanding        Add -ffreestanding (GCC does not; it also hides alloca)
  -verify              Add -mllvm -verify-machineinstrs (fail on invalid MIR;
                       catches the peephole-liveness family, e.g. ravn/llvm-z80#199).
                       Use BUILD_DIR=<assertions build> to add internal asserts.
  -diff-opt            Cross-opt-level differential: flag any test whose value
                       differs across opt levels (a miscompile regardless of the
                       `expect` directive). Strongest with -opt all. Caught ravn/llvm-z80#202.
  -native-oracle       Differential vs the host C compiler (env CC, else
                       cc/clang/gcc): flag any test whose Z80 result disagrees
                       with the host's computed value (catches consistently-wrong
                       values; reference is computed, not a hand-written expect)."
    );
}

fn print_torture_help() {
    eprintln!(
        "\
Usage: z80-test-runner torture [options] [name-filter]

  -target <z80|sm83>   Target architecture (default: z80)
  -opt <O0|O1|...|all> Optimization level (default: Os)
  -tier <compile|execute|all>  Which tier to run (default: all)
  -jobs <N>            Worker threads (default: one per core). Heavy steps are
                       capped at the core count regardless, so raising this
                       mainly lets more work queue up behind that cap.
  -std <name>          C standard passed to clang (default: gnu17)
  -emu-cycles <N>      Emulated cycles a test may spend (default: 1e10). A
                       program property, so a busy machine cannot change the
                       verdict; a separate wall-clock backstop catches an
                       emulator that has wedged rather than one that is slow.
  -run-skipped         Run ONLY the manifest's skipped tests and report any
                       that now pass, so a stale or wrong skip= cannot hide a
                       working test forever.
  -list-failures <f>   Write the failure list to a file, for diffing two runs.
  -verify              Add -mllvm -verify-machineinstrs

  The torture suite has no expected-failure mechanism on purpose. The manifest
  skips only what the target structurally cannot do (no libc, no 64-bit double,
  16-bit int, 64 KB address space); a backend bug keeps failing until fixed. A
  run whose only failures are the ones the manifest attributes to clang still
  exits successfully, since nothing here can turn those green."
    );
}

/// Whether the first argument asks for this command's own help.
fn wants_help(args: &[String]) -> bool {
    matches!(args.first().map(String::as_str), Some("help" | "-h" | "--help"))
}

fn parse_target(args: &[String], i: &mut usize) -> Target {
    *i += 1;
    if *i < args.len() {
        match args[*i].as_str() {
            "sm83" => Target::SM83,
            "z80" => Target::Z80,
            other => {
                eprintln!("unknown target: {other}, using z80");
                Target::Z80
            }
        }
    } else {
        eprintln!("-target requires an argument");
        Target::Z80
    }
}

fn cmd_run_all(args: &[String]) -> ExitCode {
    use crate::config::OptLevel;

    let mut mode = run_all::Mode::Default;
    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "-full" => mode = run_all::Mode::Full,
            "-opt" => {
                i += 1;
                if i < args.len() {
                    match OptLevel::parse(&args[i]) {
                        Some(opt) => mode = run_all::Mode::Opt(vec![opt]),
                        None => {
                            eprintln!("invalid opt level: {}", args[i]);
                            return ExitCode::FAILURE;
                        }
                    }
                }
            }
            _ => {}
        }
        i += 1;
    }

    let paths = Paths::resolve();
    if run_all::run(mode, &paths, None) {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}

/// Everything a regression check needs. `test` is the quick pass; this is the
/// one to run before deciding a change is safe.
///
/// The phases run together rather than one after another, behind a single
/// progress line. They are not competing for the machine: every expensive step
/// takes a permit from `harness::pool`, so the total running at once is the
/// core count regardless of how many threads the phases between them create.
fn cmd_full(args: &[String]) -> ExitCode {
    let _ = args;
    let paths = Paths::resolve();

    // Nothing but the progress line prints while the phases run; what they
    // would have said is held back and shown only if something failed.
    report::capture();

    // Plan the torture runs before anything starts, so the progress line is
    // sized once. run_all counts its own suites up front for the same reason.
    // Every optimisation level the torture suite is meaningful at, on both
    // targets: a backend bug often shows at one level and not the others.
    let torture: Vec<(torture::TortureConfig, torture::Plan)> = [Target::Z80, Target::SM83]
        .into_iter()
        .flat_map(|target| {
            [config::OptLevel::O1, config::OptLevel::O2, config::OptLevel::Os]
                .into_iter()
                .map(move |opt| (target, opt))
        })
        .filter_map(|(target, opt)| {
            let config = torture::TortureConfig {
                opt_levels: vec![opt],
                ..torture::TortureConfig::defaults(target)
            };
            torture::plan(&paths, &config).map(|plan| (config, plan))
        })
        .collect();

    let lit_config = lit::LitConfig { pattern: None };
    let planned: u64 = run_all::planned_total(&run_all::Mode::Full, &paths)
        + lit::count(&paths, &lit_config) as u64
        + torture.iter().map(|(_, p)| p.len() as u64).sum::<u64>();
    let progress = std::sync::Arc::new(report::Line::new(planned, "tests"));
    let ok = std::sync::atomic::AtomicBool::new(torture.len() == 6);

    std::thread::scope(|scope| {
        let mut handles = Vec::new();
        {
            let progress = &progress;
            let paths = &paths;
            let ok = &ok;
            handles.push(scope.spawn(move || {
                if !run_all::run(run_all::Mode::Full, paths, Some(progress)) {
                    ok.store(false, std::sync::atomic::Ordering::Relaxed);
                }
            }));
        }
        {
            let progress = &progress;
            let paths = &paths;
            let ok = &ok;
            let lit_config = &lit_config;
            handles.push(scope.spawn(move || {
                let result = lit::run(paths, lit_config, Some(progress));
                lit::report(&result);
                if !result.all_ok() {
                    ok.store(false, std::sync::atomic::Ordering::Relaxed);
                }
            }));
        }
        for (config, plan) in torture {
            let progress = &progress;
            let paths = &paths;
            let ok = &ok;
            handles.push(scope.spawn(move || {
                if !torture::run_planned(paths, &config, plan, Some(progress)) {
                    ok.store(false, std::sync::atomic::Ordering::Relaxed);
                }
            }));
        }
        for h in handles {
            let _ = h.join();
        }
    });

    if let Some(line) = std::sync::Arc::into_inner(progress) {
        line.clear();
    }

    let held = report::release();
    let ok = ok.into_inner();
    if ok {
        report::outcome(true, "full: ALL PASS", "");
        ExitCode::SUCCESS
    } else {
        for line in held {
            report::print_line(&line);
        }
        report::outcome(false, "", "full: SOME FAILURES");
        ExitCode::FAILURE
    }
}

fn cmd_clang(args: &[String]) -> ExitCode {
    if wants_help(args) {
        print_clang_help();
        return ExitCode::SUCCESS;
    }

    let mut target = Target::Z80;
    let mut opt_filter = "all".to_string();
    let mut fast_math = false;
    let mut omit_fp = false;
    let mut static_stack = false;
    let mut verify = false;
    let mut diff_opt = false;
    let mut native_oracle = false;
    let mut pattern = None;

    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "-target" => target = parse_target(args, &mut i),
            "-opt" => {
                i += 1;
                if i < args.len() {
                    opt_filter = args[i].clone();
                }
            }
            "-fast-math" => fast_math = true,
            "-omit-frame-pointer" => omit_fp = true,
            "-static-stack" => static_stack = true,
            "-verify" => verify = true,
            "-diff-opt" => diff_opt = true,
            "-native-oracle" => native_oracle = true,
            s if !s.starts_with('-') => pattern = Some(s.to_string()),
            _ => {}
        }
        i += 1;
    }

    let paths = Paths::resolve();
    let opt_levels = suite::expand_opt_levels(&opt_filter);

    let config = clang::ClangConfig {
        target,
        opt_levels,
        fast_math,
        omit_fp,
        inline_runtime: false,
        static_stack,
        verify,
        diff_opt,
        native_oracle,
        pattern,
    };

    let t = target.triple().to_uppercase();
    println!("{t} Backend C Test Suite");
    println!("========================");
    println!("Build:  {}", paths.build_dir.display());
    println!("Target: {target}");
    println!("Opt:    {opt_filter}");
    if fast_math {
        println!("Flags:  -ffast-math");
    }
    if omit_fp {
        println!("Flags:  -fomit-frame-pointer");
    }
    if verify {
        println!("Flags:  -verify-machineinstrs");
    }
    if diff_opt {
        println!("Flags:  -diff-opt (cross-opt-level differential)");
    }
    if native_oracle {
        println!("Flags:  -native-oracle (host C reference differential)");
    }
    println!();

    let result = clang::run(&paths, &config, &mut print_callback());

    println!();
    println!("========================");
    println!("{result}");

    if result.all_ok() {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}

fn cmd_sdcc(args: &[String]) -> ExitCode {
    let mut target = Target::Z80;
    let mut opt_filter = "all".to_string();
    let mut omit_fp = false;
    let mut pattern = None;

    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "-target" => target = parse_target(args, &mut i),
            "-opt" => {
                i += 1;
                if i < args.len() {
                    opt_filter = args[i].clone();
                }
            }
            "-omit-frame-pointer" => omit_fp = true,
            s if !s.starts_with('-') => pattern = Some(s.to_string()),
            _ => {}
        }
        i += 1;
    }

    let paths = Paths::resolve();
    let opt_levels = suite::expand_opt_levels(&opt_filter);

    let config = sdcc::SdccConfig {
        target,
        opt_levels,
        omit_fp,
        pattern,
    };

    let t = target.triple().to_uppercase();
    println!("{t} SDCC Compatibility Test Suite");
    println!("========================");
    println!("Build:  {}", paths.build_dir.display());
    println!("Target: {target}");
    println!("Opt:    {opt_filter}");
    if omit_fp {
        println!("Flags:  -fomit-frame-pointer");
    }
    println!();

    let result = sdcc::run(&paths, &config, &mut print_callback());

    println!();
    println!("========================");
    println!("{result}");

    if result.all_ok() {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}

fn cmd_lit(args: &[String]) -> ExitCode {
    let pattern = args.iter().find(|a| !a.starts_with('-')).cloned();

    let paths = Paths::resolve();
    let config = lit::LitConfig { pattern };

    report::heading("LLVM lit tests");
    report::fields(&[("Paths", lit::paths_checked(&paths).join("  "))]);

    let result = lit::run(&paths, &config, None);
    lit::report(&result);
    if result.all_ok() { ExitCode::SUCCESS } else { ExitCode::FAILURE }
}

fn cmd_llc(args: &[String]) -> ExitCode {
    let mut target = Target::Z80;
    let mut opt_filter = "all".to_string();
    let mut pattern = None;

    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "-target" => target = parse_target(args, &mut i),
            "-opt" => {
                i += 1;
                if i < args.len() {
                    opt_filter = args[i].clone();
                }
            }
            s if !s.starts_with('-') => pattern = Some(s.to_string()),
            _ => {}
        }
        i += 1;
    }

    let paths = Paths::resolve();
    let opt_levels = suite::expand_llc_opt_levels(&opt_filter);

    let config = llc::LlcConfig {
        target,
        opt_levels,
        pattern,
    };

    let t = target.triple().to_uppercase();
    println!("{t} Backend LLC Test Suite");
    println!("========================");
    println!("Build:  {}", paths.build_dir.display());
    println!("Target: {target}");
    println!("Opt:    {opt_filter}");
    println!();

    let result = llc::run(&paths, &config, &mut print_callback());

    println!();
    println!("========================");
    println!("{result}");

    if result.all_ok() {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}

fn cmd_utils(args: &[String]) -> ExitCode {
    let mut target = Target::Z80;
    let mut opt = config::OptLevel::Os;
    let mut pattern = None;

    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "-target" => target = parse_target(args, &mut i),
            "-opt" => {
                i += 1;
                if i < args.len() {
                    if let Some(o) = config::OptLevel::parse(&args[i]) {
                        opt = o;
                    } else {
                        eprintln!("invalid opt level: {}", args[i]);
                        return ExitCode::FAILURE;
                    }
                }
            }
            s if !s.starts_with('-') => pattern = Some(s.to_string()),
            _ => {}
        }
        i += 1;
    }

    let paths = Paths::resolve();
    let config = utils::UtilsConfig { target, opt, pattern };

    if utils::run_parallel(&paths, &config) {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}

fn cmd_bench(args: &[String]) -> ExitCode {
    let mut target = Target::Z80;
    let mut opt = config::OptLevel::Os;
    let mut pattern = None;

    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "-target" => target = parse_target(args, &mut i),
            "-opt" => {
                i += 1;
                if i < args.len() {
                    if let Some(o) = config::OptLevel::parse(&args[i]) {
                        opt = o;
                    } else {
                        eprintln!("invalid opt level: {}", args[i]);
                        return ExitCode::FAILURE;
                    }
                }
            }
            s if !s.starts_with('-') => pattern = Some(s.to_string()),
            _ => {}
        }
        i += 1;
    }

    let paths = Paths::resolve();
    let config = bench::BenchConfig { target, opt, pattern };
    bench::run(&paths, &config);
    ExitCode::SUCCESS
}

fn cmd_stdcbench(args: &[String]) -> ExitCode {
    let mut target = Target::Z80;
    let mut opt = config::OptLevel::Os;
    let mut iterations = 1u32;

    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "-target" => target = parse_target(args, &mut i),
            "-opt" => {
                i += 1;
                if i < args.len() {
                    match config::OptLevel::parse(&args[i]) {
                        Some(o) => opt = o,
                        None => {
                            eprintln!("invalid opt level: {}", args[i]);
                            return ExitCode::FAILURE;
                        }
                    }
                }
            }
            "-iterations" => {
                i += 1;
                match args.get(i).and_then(|s| s.parse::<u32>().ok()) {
                    Some(n) if n > 0 => iterations = n,
                    _ => {
                        eprintln!("-iterations expects a positive number");
                        return ExitCode::FAILURE;
                    }
                }
            }
            _ => {}
        }
        i += 1;
    }

    let paths = Paths::resolve();
    let config = stdcbench::StdcbenchConfig { target, opt, iterations };
    if stdcbench::run(&paths, &config) {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}

fn cmd_torture(args: &[String]) -> ExitCode {
    if wants_help(args) {
        print_torture_help();
        return ExitCode::SUCCESS;
    }

    let mut target = Target::Z80;
    let mut opt_filter = "Os".to_string();
    let mut tiers = vec![torture::Tier::Compile, torture::Tier::Execute];
    let mut jobs = harness::pool::capacity();
    let mut std_name = "gnu17".to_string();
    let mut emu_cycles = torture::EMU_CYCLES;
    let mut freestanding = false;
    let mut verify = false;
    let mut run_skipped = false;
    let mut list_failures = None;
    let mut pattern = None;

    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "-target" => target = parse_target(args, &mut i),
            "-opt" => {
                i += 1;
                if i < args.len() {
                    opt_filter = args[i].clone();
                }
            }
            "-tier" => {
                i += 1;
                match args.get(i).and_then(|s| torture::Tier::parse(s)) {
                    Some(t) => tiers = t,
                    None => {
                        eprintln!("-tier expects compile, execute or all");
                        return ExitCode::FAILURE;
                    }
                }
            }
            "-jobs" => {
                i += 1;
                match args.get(i).and_then(|s| s.parse::<usize>().ok()) {
                    Some(n) if n > 0 => jobs = n,
                    _ => {
                        eprintln!("-jobs expects a positive number");
                        return ExitCode::FAILURE;
                    }
                }
            }
            "-std" => {
                i += 1;
                if i < args.len() {
                    std_name = args[i].clone();
                }
            }
            "-list-failures" => {
                i += 1;
                match args.get(i) {
                    Some(p) => list_failures = Some(std::path::PathBuf::from(p)),
                    None => {
                        eprintln!("-list-failures expects a path");
                        return ExitCode::FAILURE;
                    }
                }
            }
            "-emu-cycles" => {
                i += 1;
                match args.get(i).and_then(|s| s.parse::<u64>().ok()) {
                    Some(n) if n > 0 => emu_cycles = n,
                    _ => {
                        eprintln!("-emu-cycles expects a positive number of cycles");
                        return ExitCode::FAILURE;
                    }
                }
            }
            "-freestanding" => freestanding = true,
            "-verify" => verify = true,
            "-run-skipped" => run_skipped = true,
            // `cargo run torture -- -tier ...` forwards the separator too.
            "--" => {}
            s if !s.starts_with('-') => pattern = Some(s.to_string()),
            other => {
                eprintln!("unknown option: {other}");
                return ExitCode::FAILURE;
            }
        }
        i += 1;
    }

    let opt_levels = suite::expand_opt_levels(&opt_filter);
    if opt_levels.is_empty() {
        eprintln!("invalid opt level: {opt_filter}");
        return ExitCode::FAILURE;
    }

    let paths = Paths::resolve();
    let config = torture::TortureConfig {
        target,
        opt_levels,
        tiers,
        jobs,
        pattern,
        freestanding,
        verify,
        run_skipped,
        list_failures,
        std: std_name,
        emu_cycles,
    };

    if torture::run(&paths, &config, None) {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}
