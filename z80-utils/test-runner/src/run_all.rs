use std::io::{self, Write};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use crate::clang::ClangConfig;
use crate::config::{OptLevel, Paths, Target};
use crate::custom::{self, CustomConfig};
use crate::display;
use crate::llc::LlcConfig;
use crate::sdcc::SdccConfig;
use crate::suite::{OnResult, SuiteResult};
use crate::utils::{self, UtilsConfig};
use crate::{clang, llc, sdcc};

#[derive(Clone, Debug, PartialEq)]
pub enum Mode {
    Default,
    Full,
    Opt(Vec<OptLevel>),
}

/// Per-suite progress state, shared between worker threads and the display thread.
struct SuiteState {
    label: String,
    done: u32,      // tests completed so far
    total: u32,     // total tests in this suite (set by runner after discovery)
    result: Option<SuiteResult>, // set when suite finishes
}

type SharedState = Arc<Mutex<Vec<SuiteState>>>;

type SuiteRunner = Box<dyn FnOnce(&Paths, SharedState, usize) + Send>;

struct SuiteDef {
    label: String,
    runner: SuiteRunner,
}

pub fn run(mode: Mode, paths: &Paths) -> bool {
    let suites = build_suites(&mode);
    let num = suites.len();

    let state: SharedState = Arc::new(Mutex::new(
        suites.iter().map(|s| SuiteState {
            label: s.label.clone(),
            done: 0,
            total: 0,
            result: None,
        }).collect()
    ));

    let paths = Arc::new(paths.clone());

    // Launch all suites in parallel
    let _handles: Vec<_> = suites
        .into_iter()
        .enumerate()
        .map(|(i, suite)| {
            let state = Arc::clone(&state);
            let paths = Arc::clone(&paths);
            thread::spawn(move || {
                (suite.runner)(&paths, state, i);
            })
        })
        .collect();

    // Display loop
    let tty = display::is_tty();

    if tty {
        // Print header + initial lines
        print!("\x1b[?25l"); // hide cursor
        print!("\x1b[1mZ80 Backend Test Results\x1b[0m\n");
        print!("========================\n");
        {
            let lock = state.lock().unwrap();
            for s in lock.iter() {
                print!("  \x1b[2m\u{22ef} {}  -\x1b[0m\n", s.label);
            }
        }
        print!("========================\n");
        let _ = io::stdout().flush();

        let total_lines = 2 + num + 1; // header(2) + suites + separator

        loop {
            thread::sleep(Duration::from_millis(100));

            let lock = state.lock().unwrap();
            let finished = lock.iter().filter(|s| s.result.is_some()).count();

            // Move cursor up and redraw
            print!("\x1b[{total_lines}A");
            print!("\x1b[1mZ80 Backend Test Results\x1b[0m\n");
            print!("========================\n");
            for s in lock.iter() {
                render_suite_line_tty(s);
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
        // Non-TTY: poll, print completed suites as they finish (any order)
        let mut printed = vec![false; num];
        println!("Z80 Backend Test Results");
        println!("========================");

        loop {
            thread::sleep(Duration::from_millis(100));
            let lock = state.lock().unwrap();
            for (i, s) in lock.iter().enumerate() {
                if !printed[i] {
                    if let Some(ref r) = s.result {
                        print_suite_line_plain(&s.label, r);
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

    // Aggregate
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
                        crate::suite::TestOutcome::Fail { got, expected } => {
                            println!("  FAIL  {}  (got {got}, expected {expected})", t.tag);
                        }
                        crate::suite::TestOutcome::Fatal { reason } => {
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

fn render_suite_line_tty(s: &SuiteState) {
    // Use \x1b[2K to clear the line first (handles shrinking text)
    print!("\x1b[2K");
    match &s.result {
        None => {
            // In progress
            if s.total > 0 {
                print!("  \x1b[2m\u{22ef} {}  [{}/{}]\x1b[0m\n", s.label, s.done, s.total);
            } else {
                print!("  \x1b[2m\u{22ef} {}  ...\x1b[0m\n", s.label);
            }
        }
        Some(r) => {
            if !r.all_ok() {
                print!("  \x1b[31m\u{2717}\x1b[0m {}  {}/{}", s.label, r.pass, r.total);
                if r.fail > 0 {
                    print!("  \x1b[31mfail={}\x1b[0m", r.fail);
                }
                if r.fatal > 0 {
                    print!("  \x1b[31mfatal={}\x1b[0m", r.fatal);
                }
                println!();
            } else {
                print!("  \x1b[32m\u{2713}\x1b[0m {}  {}/{}", s.label, r.pass, r.total);
                if r.skip > 0 {
                    print!("  \x1b[33mskip={}\x1b[0m", r.skip);
                }
                println!();
            }
        }
    }
}

fn print_suite_line_plain(label: &str, r: &SuiteResult) {
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

/// Create a callback that increments the progress counter for suite `idx`.
fn progress_callback(state: SharedState, idx: usize) -> OnResult {
    Box::new(move |_result, _reg| {
        let mut lock = state.lock().unwrap();
        lock[idx].done += 1;
    })
}

fn build_suites(mode: &Mode) -> Vec<SuiteDef> {
    let mut suites = Vec::new();

    let all_opts = vec![OptLevel::O0, OptLevel::O1, OptLevel::O2, OptLevel::O3, OptLevel::Os, OptLevel::Oz];

    match mode {
        Mode::Opt(opts) => {
            let label_suffix = opts.iter().map(|o| o.clang_flag()).collect::<Vec<_>>().join(",");
            add_clang(&mut suites, &format!("clang Z80 {label_suffix}"), Target::Z80, opts.clone(), false, false);
            add_clang(&mut suites, &format!("clang SM83 {label_suffix}"), Target::SM83, opts.clone(), false, false);
            add_clang(&mut suites, &format!("clang Z80 -omit-fp {label_suffix}"), Target::Z80, opts.clone(), false, true);
            add_clang_filtered(&mut suites, &format!("clang Z80 -ffast-math {label_suffix}"), Target::Z80, opts.clone(), true, false, Some("f32".into()));
            add_clang_filtered(&mut suites, &format!("clang SM83 -ffast-math {label_suffix}"), Target::SM83, opts.clone(), true, false, Some("f32".into()));
            add_clang_inline_rt(&mut suites, &format!("clang Z80 -inline-i16-rt {label_suffix}"), Target::Z80, opts.clone());
            add_clang_inline_rt(&mut suites, &format!("clang SM83 -inline-i16-rt {label_suffix}"), Target::SM83, opts.clone());
        }
        Mode::Full => {
            add_clang(&mut suites, "clang Z80 (all)", Target::Z80, all_opts.clone(), false, false);
            add_clang(&mut suites, "clang SM83 (all)", Target::SM83, all_opts.clone(), false, false);
            add_clang(&mut suites, "clang Z80 -omit-fp (all)", Target::Z80, all_opts.clone(), false, true);
            add_clang_filtered(&mut suites, "clang Z80 -ffast-math (all)", Target::Z80, all_opts.clone(), true, false, Some("f32".into()));
            add_clang_filtered(&mut suites, "clang SM83 -ffast-math (all)", Target::SM83, all_opts.clone(), true, false, Some("f32".into()));
            add_clang_inline_rt(&mut suites, "clang Z80 -inline-i16-rt (all)", Target::Z80, all_opts.clone());
            add_clang_inline_rt(&mut suites, "clang SM83 -inline-i16-rt (all)", Target::SM83, all_opts.clone());
        }
        Mode::Default => {
            add_clang(&mut suites, "clang Z80 O1", Target::Z80, vec![OptLevel::O1], false, false);
            add_clang(&mut suites, "clang Z80 O2", Target::Z80, vec![OptLevel::O2], false, false);
            add_clang(&mut suites, "clang Z80 Os", Target::Z80, vec![OptLevel::Os], false, false);
            add_clang(&mut suites, "clang SM83 O1", Target::SM83, vec![OptLevel::O1], false, false);
            add_clang(&mut suites, "clang SM83 O2", Target::SM83, vec![OptLevel::O2], false, false);
            add_clang(&mut suites, "clang SM83 Os", Target::SM83, vec![OptLevel::Os], false, false);
            add_clang(&mut suites, "clang Z80 -omit-fp", Target::Z80, vec![OptLevel::O1], false, true);
            add_clang_filtered(&mut suites, "clang Z80 -ffast-math", Target::Z80, vec![OptLevel::O1], true, false, Some("f32".into()));
            add_clang_filtered(&mut suites, "clang SM83 -ffast-math", Target::SM83, vec![OptLevel::O1], true, false, Some("f32".into()));
            add_clang_inline_rt(&mut suites, "clang Z80 -inline-i16-rt", Target::Z80, vec![OptLevel::O1]);
            add_clang_inline_rt(&mut suites, "clang SM83 -inline-i16-rt", Target::SM83, vec![OptLevel::O1]);
        }
    }

    let llc_opts = match mode {
        Mode::Opt(opts) => opts.clone(),
        _ => vec![OptLevel::O0, OptLevel::O1],
    };

    let sdcc_opts = match mode {
        Mode::Full => all_opts,
        Mode::Opt(opts) => opts.clone(),
        Mode::Default => vec![OptLevel::Os],
    };

    add_sdcc(&mut suites, &format_suite_label("sdcc Z80", &sdcc_opts), Target::Z80, sdcc_opts.clone(), false);
    add_sdcc(&mut suites, &format_suite_label("sdcc SM83", &sdcc_opts), Target::SM83, sdcc_opts.clone(), false);
    add_sdcc(&mut suites, &format_suite_label("sdcc Z80 -omit-fp", &sdcc_opts), Target::Z80, sdcc_opts, true);
    add_llc(&mut suites, &format_suite_label("llc Z80", &llc_opts), Target::Z80, llc_opts.clone());
    add_llc(&mut suites, &format_suite_label("llc SM83", &llc_opts), Target::SM83, llc_opts);

    add_custom(&mut suites, "custom Z80", Target::Z80);
    add_custom(&mut suites, "custom SM83", Target::SM83);

    if matches!(mode, Mode::Full) {
        add_utils(&mut suites, "utils Z80", Target::Z80);
        add_utils(&mut suites, "utils SM83", Target::SM83);
    }

    suites
}

fn format_suite_label(prefix: &str, opts: &[OptLevel]) -> String {
    if opts.len() > 2 {
        format!("{prefix} (all)")
    } else {
        let suffix = opts.iter().map(|o| o.clang_flag()).collect::<Vec<_>>().join(",");
        format!("{prefix} {suffix}")
    }
}

fn add_clang(
    suites: &mut Vec<SuiteDef>,
    label: &str,
    target: Target,
    opts: Vec<OptLevel>,
    fast_math: bool,
    omit_fp: bool,
) {
    add_clang_filtered(suites, label, target, opts, fast_math, omit_fp, None);
}

fn add_clang_filtered(
    suites: &mut Vec<SuiteDef>,
    label: &str,
    target: Target,
    opts: Vec<OptLevel>,
    fast_math: bool,
    omit_fp: bool,
    pattern: Option<String>,
) {
    let label = label.to_string();
    suites.push(SuiteDef {
        label: label.clone(),
        runner: Box::new(move |paths, state, idx| {
            let config = ClangConfig { target, opt_levels: opts, fast_math, omit_fp, inline_runtime: false, pattern };
            // Pre-count tests for progress display
            let test_dir = paths.clang_test_dir();
            let tests = crate::suite::discover_tests(&test_dir, "test_", "c");
            let count = tests.iter()
                .filter(|t| {
                    let name = t.file_stem().unwrap().to_string_lossy();
                    config.pattern.as_ref().map_or(true, |p| name.contains(p.as_str()))
                })
                .count() as u32 * config.opt_levels.len() as u32;
            state.lock().unwrap()[idx].total = count;

            let mut cb = progress_callback(state.clone(), idx);
            let result = clang::run(paths, &config, &mut cb);
            state.lock().unwrap()[idx].result = Some(result);
        }),
    });
}

fn add_clang_inline_rt(
    suites: &mut Vec<SuiteDef>,
    label: &str,
    target: Target,
    opts: Vec<OptLevel>,
) {
    let label = label.to_string();
    suites.push(SuiteDef {
        label: label.clone(),
        runner: Box::new(move |paths, state, idx| {
            let config = ClangConfig {
                target, opt_levels: opts, fast_math: false, omit_fp: false,
                inline_runtime: true, pattern: None,
            };
            let test_dir = paths.clang_test_dir();
            let tests = crate::suite::discover_tests(&test_dir, "test_", "c");
            let count = tests.len() as u32 * config.opt_levels.len() as u32;
            state.lock().unwrap()[idx].total = count;

            let mut cb = progress_callback(state.clone(), idx);
            let result = clang::run(paths, &config, &mut cb);
            state.lock().unwrap()[idx].result = Some(result);
        }),
    });
}

fn add_sdcc(
    suites: &mut Vec<SuiteDef>,
    label: &str,
    target: Target,
    opts: Vec<OptLevel>,
    omit_fp: bool,
) {
    let label = label.to_string();
    let num_opts = opts.len() as u32;
    suites.push(SuiteDef {
        label: label.clone(),
        runner: Box::new(move |paths, state, idx| {
            // Pre-count: test_*_clang.c files × opt levels
            let test_dir = paths.sdcc_test_dir();
            let count = std::fs::read_dir(&test_dir)
                .into_iter().flatten().filter_map(|e| e.ok())
                .filter(|e| {
                    let n = e.file_name().to_string_lossy().to_string();
                    n.starts_with("test_") && n.ends_with("_clang.c")
                })
                .count() as u32 * num_opts;
            state.lock().unwrap()[idx].total = count;

            let mut cb = progress_callback(state.clone(), idx);
            let result = sdcc::run(
                paths,
                &SdccConfig { target, opt_levels: opts, omit_fp, pattern: None },
                &mut cb,
            );
            state.lock().unwrap()[idx].result = Some(result);
        }),
    });
}

fn add_llc(
    suites: &mut Vec<SuiteDef>,
    label: &str,
    target: Target,
    opts: Vec<OptLevel>,
) {
    let label = label.to_string();
    let num_opts = opts.len() as u32;
    suites.push(SuiteDef {
        label: label.clone(),
        runner: Box::new(move |paths, state, idx| {
            // Pre-count: test_*.ll files × opt levels
            let test_dir = paths.llc_test_dir();
            let count = crate::suite::discover_tests(&test_dir, "test_", "ll").len() as u32 * num_opts;
            state.lock().unwrap()[idx].total = count;

            let mut cb = progress_callback(state.clone(), idx);
            let result = llc::run(
                paths,
                &LlcConfig { target, opt_levels: opts, pattern: None },
                &mut cb,
            );
            state.lock().unwrap()[idx].result = Some(result);
        }),
    });
}

fn add_custom(
    suites: &mut Vec<SuiteDef>,
    label: &str,
    target: Target,
) {
    let label = label.to_string();
    suites.push(SuiteDef {
        label: label.clone(),
        runner: Box::new(move |paths, state, idx| {
            let dir = paths.custom_test_dir();
            let files = custom::discover_files(&dir);
            let count = files.len() as u32;
            state.lock().unwrap()[idx].total = count;

            if files.is_empty() {
                state.lock().unwrap()[idx].result = Some(SuiteResult::default());
                return;
            }

            let file_strs: Vec<String> = files.iter()
                .map(|p| p.to_string_lossy().to_string())
                .collect();
            let config = CustomConfig {
                target,
                opt: OptLevel::O1,
                files: file_strs,
            };
            let mut cb = progress_callback(state.clone(), idx);
            let result = custom::run(paths, &config, &mut cb);
            state.lock().unwrap()[idx].result = Some(result);
        }),
    });
}

fn add_utils(
    suites: &mut Vec<SuiteDef>,
    label: &str,
    target: Target,
) {
    let label = label.to_string();
    suites.push(SuiteDef {
        label: label.clone(),
        runner: Box::new(move |paths, state, idx| {
            // Pre-count: 6 groups × number of clang test files
            let test_dir = paths.clang_test_dir();
            let test_count = crate::suite::discover_tests(&test_dir, "test_", "c").len() as u32;
            state.lock().unwrap()[idx].total = test_count * 6;

            let config = UtilsConfig { target, opt: OptLevel::O1, pattern: None };
            let mut cb = progress_callback(state.clone(), idx);
            let result = utils::run(paths, &config, &mut cb);
            state.lock().unwrap()[idx].result = Some(result);
        }),
    });
}
