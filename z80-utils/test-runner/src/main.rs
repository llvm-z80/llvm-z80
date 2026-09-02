mod bench;
mod clang;
mod config;
mod custom;
mod display;
mod emulator;
mod llc;
mod run_all;
mod runtime;
mod sdcc;
mod suite;
mod torture;
mod torture_data;
mod utils;

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

    if args.is_empty() {
        return cmd_run_all(&args);
    }

    if args[0].starts_with('-') && args[0] != "--help" && args[0] != "-h" {
        return cmd_run_all(&args);
    }

    match args[0].as_str() {
        "bench" => cmd_bench(&args[1..]),
        "clang" => cmd_clang(&args[1..]),
        "custom" => cmd_custom(&args[1..]),
        "sdcc" => cmd_sdcc(&args[1..]),
        "torture" => cmd_torture(&args[1..]),
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
Usage: z80-test-runner [command] [options]

Commands:
  (none)     Run all test suites in parallel (default)
  bench      Run Clang vs SDCC benchmark comparison
  clang      Run Clang C test suite
  custom     Compile-check arbitrary .c or .ll files
  sdcc       Run SDCC compatibility test suite
  torture    Run the GCC C torture suite (not part of the default run)
  llc        Run LLC (LLVM IR) test suite
  utils      Run elf2rel/rel2elf roundtrip and crosslink tests
  help       Show this help

Run-all options:
  -full      Run all opt levels (O0/O1/O2/O3/Os/Oz)
  -opt <LVL> Run only the specified opt level

Suite options:
  -target <z80|sm83>   Target architecture (default: z80)
  -opt <O0|O1|...|all> Optimization level (default: all)

Clang-specific:
  -fast-math           Enable -ffast-math
  -omit-frame-pointer  Enable -fomit-frame-pointer
  -static-stack        Enable +static-stack (BSS locals)
  -freestanding        Add -ffreestanding (GCC does not; it also hides alloca)
  -verify              Add -mllvm -verify-machineinstrs (fail on invalid MIR;
                       catches the peephole-liveness family, e.g. ravn/llvm-z80#199).
                       Use BUILD_DIR=<assertions build> to add internal asserts.
  -diff-opt            Cross-opt-level differential: flag any test whose value
                       differs across opt levels (a miscompile regardless of the
                       `expect` directive). Strongest with -full. Caught ravn/llvm-z80#202.
  -native-oracle       Differential vs the host C compiler (env CC, else
                       cc/clang/gcc): flag any test whose Z80 result disagrees
                       with the host's computed value (catches consistently-wrong
                       values; reference is computed, not a hand-written expect).

Torture-specific:
  -tier <compile|execute|all>  Which tier to run (default: all)
  -jobs <N>            Parallel workers (default: 20)
  -std <name>          C standard passed to clang (default: gnu17)
  -emu-timeout <SECS>  Emulator budget per test (default: 5). Raise it to tell
                       a real hang apart from a merely slow test.
  -run-skipped         Run ONLY the manifest's skipped tests and report any
                       that now pass, so a stale or wrong skip= cannot hide a
                       working test forever.
  -list-failures <f>   Write the failure list to a file, for diffing two runs.
  -verify              Add -mllvm -verify-machineinstrs

  The torture suite has no expected-failure mechanism on purpose. The manifest
  skips only what the target structurally cannot do (no libc, no 64-bit double,
  16-bit int, 64 KB address space); a backend bug keeps failing until fixed, so
  this suite stays red while any bug is outstanding and is excluded from the
  default run.

Environment:
  BUILD_DIR            Build directory (default: ../build)"
    );
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
    if run_all::run(mode, &paths) {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}

fn cmd_clang(args: &[String]) -> ExitCode {
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

fn cmd_custom(args: &[String]) -> ExitCode {
    let mut target = Target::Z80;
    let mut opt = config::OptLevel::O1;
    let mut files = Vec::new();

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
            s if !s.starts_with('-') => files.push(s.to_string()),
            _ => {}
        }
        i += 1;
    }

    let paths = Paths::resolve();

    // If no files given, discover from testcases/custom/
    if files.is_empty() {
        let dir = paths.custom_test_dir();
        files = custom::discover_files(&dir)
            .into_iter()
            .map(|p| p.to_string_lossy().to_string())
            .collect();
    }

    if files.is_empty() {
        eprintln!("no .c or .ll files found");
        return ExitCode::FAILURE;
    }

    let config = custom::CustomConfig { target, opt, files };
    let result = custom::run(&paths, &config, &mut print_callback());

    if result.all_ok() {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}

fn cmd_utils(args: &[String]) -> ExitCode {
    let mut target = Target::Z80;
    let mut opt = config::OptLevel::O1;
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
    let mut opt = config::OptLevel::O1;
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

fn cmd_torture(args: &[String]) -> ExitCode {
    let mut target = Target::Z80;
    let mut opt_filter = "O1".to_string();
    let mut tiers = vec![torture::Tier::Compile, torture::Tier::Execute];
    let mut jobs = 20usize;
    let mut std_name = "gnu17".to_string();
    let mut emu_timeout = torture::EMU_TIMEOUT;
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
            "-emu-timeout" => {
                i += 1;
                match args.get(i).and_then(|s| s.parse::<u64>().ok()) {
                    Some(n) if n > 0 => emu_timeout = n,
                    _ => {
                        eprintln!("-emu-timeout expects a positive number of seconds");
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
        emu_timeout,
    };

    if torture::run(&paths, &config) {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}
