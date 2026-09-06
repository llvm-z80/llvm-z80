use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use crate::clang::ClangConfig;
use crate::config::{OptLevel, Paths, Target};
use crate::display;
use crate::report;
use crate::llc::LlcConfig;
use owo_colors::OwoColorize;
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

type SuiteRunner = Box<dyn FnOnce(&Paths, SharedState, usize, Option<&Arc<report::Line>>) + Send>;

struct SuiteDef {
    label: String,
    /// How many results this suite will produce, answerable before it runs.
    /// Both the per-suite line and the shared progress total come from here,
    /// so a caller can size a bar up front instead of watching it grow.
    count: Box<dyn FnOnce(&Paths) -> u32 + Send>,
    runner: SuiteRunner,
}

/// `shared` replaces this run's own progress display with a line that spans
/// several phases, which is what `full` needs; the per-suite result lines are
/// still printed as they settle.
///
/// A shared line must already account for `planned_total`; this does not add
/// to it, so every phase's size is contributed once, by whoever measured it.
/// How many results a run of `mode` will produce, without running any of it.
pub fn planned_total(mode: &Mode, paths: &Paths) -> u64 {
    build_suites(mode)
        .into_iter()
        .map(|s| (s.count)(paths) as u64)
        .sum()
}

pub fn run(mode: Mode, paths: &Paths, shared: Option<&Arc<report::Line>>) -> bool {
    let suites = build_suites(&mode);
    let num = suites.len();

    // Ask every suite how much work it has before starting any of it. A shared
    // line can then be sized once, rather than growing each time a suite gets
    // round to answering.
    let (counts, runners): (Vec<_>, Vec<_>) = suites
        .into_iter()
        .map(|s| ((s.label, (s.count)(paths)), s.runner))
        .unzip();

    let state: SharedState = Arc::new(Mutex::new(
        counts.into_iter().map(|(label, total)| SuiteState {
            label,
            done: 0,
            total,
            result: None,
        }).collect()
    ));

    let paths = Arc::new(paths.clone());

    // Launch all suites in parallel
    let _handles: Vec<_> = runners
        .into_iter()
        .enumerate()
        .map(|(i, runner)| {
            let state = Arc::clone(&state);
            let paths = Arc::clone(&paths);
            let shared = shared.cloned();
            thread::spawn(move || {
                runner(&paths, state, i, shared.as_ref());
            })
        })
        .collect();

    // Display loop. `report::Progress` owns the redraw; this only decides
    // what each line says.
    let labels: Vec<String> = state.lock().unwrap().iter().map(|s| s.label.clone()).collect();
    let mut progress = match shared {
        Some(_) => report::Progress::plain("Z80 Backend Test Results", &labels),
        None => report::Progress::new("Z80 Backend Test Results", &labels),
    };

    loop {
        thread::sleep(Duration::from_millis(100));

        let lock = state.lock().unwrap();
        for (i, s) in lock.iter().enumerate() {
            match &s.result {
                Some(r) => progress.finish(i, suite_line(&s.label, r)),
                None => progress.update(i, in_progress_line(s)),
            }
        }
        let finished = lock.iter().filter(|s| s.result.is_some()).count();
        drop(lock);
        if finished == num {
            break;
        }
    }
    progress.done();

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
                            crate::say!("  FAIL  {}  (got {got}, expected {expected})", t.tag);
                        }
                        crate::suite::TestOutcome::Fatal { reason } => {
                            crate::say!("  FATAL {}  ({reason})", t.tag);
                        }
                        _ => {}
                    }
                }
            }
        }
    }

    all_ok
}

/// The line a suite shows while it is still running.
fn in_progress_line(s: &SuiteState) -> String {
    let body = if s.total > 0 {
        format!("  \u{22ef} {}  [{}/{}]", s.label, s.done, s.total)
    } else {
        format!("  \u{22ef} {}  ...", s.label)
    };
    format!("{}", body.style(display::faint()))
}

/// The line a suite settles on. One wording for every stream: the styling is
/// dropped automatically when there is no terminal, so there is no second
/// plain-text copy to keep in step.
fn suite_line(label: &str, r: &SuiteResult) -> String {
    let mut line = if r.all_ok() {
        format!("  {} {label}  {}/{}", "ok".style(display::ok()), r.pass, r.total)
    } else {
        format!("  {} {label}  {}/{}", "x".style(display::bad()), r.pass, r.total)
    };
    if r.fail > 0 {
        line.push_str(&format!("  {}", format_args!("fail={}", r.fail).style(display::bad())));
    }
    if r.fatal > 0 {
        line.push_str(&format!("  {}", format_args!("fatal={}", r.fatal).style(display::bad())));
    }
    if r.skip > 0 {
        line.push_str(&format!("  skip={}", r.skip));
    }
    line
}

/// Create a callback that increments the progress counter for suite `idx`.
fn progress_callback(
    state: SharedState,
    idx: usize,
    shared: Option<Arc<report::Line>>,
) -> OnResult {
    Box::new(move |_result, _reg| {
        let mut lock = state.lock().unwrap();
        lock[idx].done += 1;
        drop(lock);
        if let Some(line) = &shared {
            line.inc();
        }
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
            add_clang(&mut suites, "clang Z80 -omit-fp", Target::Z80, vec![OptLevel::Os], false, true);
            add_clang_filtered(&mut suites, "clang Z80 -ffast-math", Target::Z80, vec![OptLevel::Os], true, false, Some("f32".into()));
            add_clang_filtered(&mut suites, "clang SM83 -ffast-math", Target::SM83, vec![OptLevel::Os], true, false, Some("f32".into()));
            add_clang_inline_rt(&mut suites, "clang Z80 -inline-i16-rt", Target::Z80, vec![OptLevel::Os]);
            add_clang_inline_rt(&mut suites, "clang SM83 -inline-i16-rt", Target::SM83, vec![OptLevel::Os]);
        }
    }

    let llc_opts = match mode {
        Mode::Opt(opts) => opts.clone(),
        // Not Os: llc rejects it outright ("invalid optimization level"),
        // which is why `expand_llc_opt_levels` stops at O1 as well.
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
    let config = ClangConfig { target, opt_levels: opts, fast_math, omit_fp, inline_runtime: false, static_stack: false, verify: false, diff_opt: false, native_oracle: false, pattern };
    let for_count = config.clone();
    suites.push(SuiteDef {
        label: label.clone(),
        count: Box::new(move |paths| clang::count(paths, &for_count)),
        runner: Box::new(move |paths, state, idx, shared| {
            let _ = shared;
            let mut cb = progress_callback(state.clone(), idx, shared.cloned());
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
    let config = ClangConfig {
                target, opt_levels: opts, fast_math: false, omit_fp: false,
                inline_runtime: true, static_stack: false, verify: false, diff_opt: false, native_oracle: false, pattern: None,
            };
    let for_count = config.clone();
    suites.push(SuiteDef {
        label: label.clone(),
        count: Box::new(move |paths| clang::count(paths, &for_count)),
        runner: Box::new(move |paths, state, idx, shared| {
            let _ = shared;
            let mut cb = progress_callback(state.clone(), idx, shared.cloned());
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
    let config = SdccConfig { target, opt_levels: opts, omit_fp, pattern: None };
    let for_count = config.clone();
    suites.push(SuiteDef {
        label: label.clone(),
        count: Box::new(move |paths| sdcc::count(paths, &for_count)),
        runner: Box::new(move |paths, state, idx, shared| {
            let _ = shared;
            let mut cb = progress_callback(state.clone(), idx, shared.cloned());
            let result = sdcc::run(paths, &config, &mut cb);
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
    let config = LlcConfig { target, opt_levels: opts, pattern: None };
    let for_count = config.clone();
    suites.push(SuiteDef {
        label: label.clone(),
        count: Box::new(move |paths| llc::count(paths, &for_count)),
        runner: Box::new(move |paths, state, idx, shared| {
            let _ = shared;
            let mut cb = progress_callback(state.clone(), idx, shared.cloned());
            let result = llc::run(paths, &config, &mut cb);
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
    let config = UtilsConfig { target, opt: OptLevel::Os, pattern: None };
    let for_count = config.clone();
    suites.push(SuiteDef {
        label: label.clone(),
        count: Box::new(move |paths| utils::count(paths, &for_count)),
        runner: Box::new(move |paths, state, idx, shared| {
            let _ = shared;
            let mut cb = progress_callback(state.clone(), idx, shared.cloned());
            let result = utils::run(paths, &config, &mut cb);
            state.lock().unwrap()[idx].result = Some(result);
        }),
    });
}
