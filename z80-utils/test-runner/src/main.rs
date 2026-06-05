mod bench;
mod clang;
mod config;
mod custom;
mod display;
mod emulator;
mod llc;
mod run_all;
mod sdcc;
mod suite;
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
