//! LLVM's own lit tests for this backend.
//!
//! These check the compiler's output directly, against `CHECK` lines in the
//! test, rather than by running the program. That makes them the only suite
//! here that can pin down *how* something is compiled and not merely that the
//! answer came out right, so a peephole that quietly stops firing shows up
//! here and nowhere else.
//!
//! Only the paths this project owns are run. The rest of LLVM's test tree is
//! upstream's business and takes far longer than everything else here put
//! together.

use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};

use crate::config::Paths;
use crate::display;
use crate::report;

/// Test paths this project owns, relative to the repository root.
const PATHS: &[&str] = &[
    "llvm/test/CodeGen/Z80",
    "llvm/test/MC/Z80",
    "clang/test/CodeGen/z80-asm-label.c",
    "clang/test/CodeGen/z80-struct-vararg.c",
    "clang/test/Driver/z80-include-paths.c",
];

pub struct LitConfig {
    /// Run only tests whose name contains this.
    pub pattern: Option<String>,
}

pub struct LitResult {
    pub total: u32,
    pub passed: u32,
    /// Name and reason for each test that did not pass, in lit's order.
    pub failures: Vec<(String, String)>,
}

impl LitResult {
    pub fn all_ok(&self) -> bool {
        self.failures.is_empty() && self.total > 0
    }
}

/// The repository root, which is where the test paths are rooted.
fn repo_root(paths: &Paths) -> PathBuf {
    paths.build_dir.join("..")
}

fn lit(paths: &Paths) -> PathBuf {
    paths.build_dir.join("bin/llvm-lit")
}

fn test_args(paths: &Paths) -> Vec<PathBuf> {
    let root = repo_root(paths);
    PATHS.iter().map(|p| root.join(p)).filter(|p| p.exists()).collect()
}

/// How many tests a run will report, asked of lit rather than counted here so
/// the number cannot drift from what actually runs.
pub fn count(paths: &Paths, config: &LitConfig) -> u32 {
    let mut cmd = Command::new("python3");
    cmd.arg(lit(paths)).arg("--show-tests");
    if let Some(pat) = &config.pattern {
        cmd.arg("--filter").arg(pat);
    }
    cmd.args(test_args(paths));
    cmd.stderr(Stdio::null());

    let Ok(out) = cmd.output() else { return 0 };
    String::from_utf8_lossy(&out.stdout)
        .lines()
        .filter(|l| l.starts_with("  ") && l.contains("::"))
        .count() as u32
}

pub fn run(paths: &Paths, config: &LitConfig, progress: Option<&report::Line>) -> LitResult {
    let mut result = LitResult { total: 0, passed: 0, failures: Vec::new() };

    let tests = test_args(paths);
    if tests.is_empty() {
        return result;
    }

    let mut cmd = Command::new("python3");
    cmd.arg(lit(paths)).arg("--no-progress-bar");
    if let Some(pat) = &config.pattern {
        cmd.arg("--filter").arg(pat);
    }
    cmd.args(&tests);
    // lit runs a great many very short processes. Left to itself it would size
    // its own pool to the machine and fight whatever else is running, so it is
    // given a modest slice; the whole suite finishes in about a second either
    // way.
    cmd.args(["-j", "4"]);
    cmd.stdout(Stdio::piped());
    cmd.stderr(Stdio::null());

    let Ok(out) = cmd.output() else { return result };
    for line in String::from_utf8_lossy(&out.stdout).lines() {
        // "PASS: LLVM :: CodeGen/Z80/foo.ll (3 of 77)"
        let Some((verdict, rest)) = line.split_once(": ") else { continue };
        let name = match rest.split_once(" (") {
            Some((name, _)) => name,
            None => continue,
        };
        match verdict {
            "PASS" | "XFAIL" => {
                result.total += 1;
                result.passed += 1;
            }
            "UNSUPPORTED" | "SKIPPED" => {
                result.total += 1;
            }
            "FAIL" | "XPASS" | "TIMEOUT" | "UNRESOLVED" => {
                result.total += 1;
                result.failures.push((name.to_string(), verdict.to_string()));
            }
            _ => continue,
        }
        if let Some(line) = progress {
            line.inc();
        }
    }
    result
}

/// Print what a standalone `lit` run found.
pub fn report(result: &LitResult) {
    use owo_colors::OwoColorize;

    for (name, verdict) in &result.failures {
        crate::say!("  {}  {name}", verdict.style(display::bad()));
    }

    report::tally(&[
        report::Count {
            label: "PASS".into(),
            style: display::ok(),
            n: result.passed as usize,
        },
        report::Count {
            label: "FAIL".into(),
            style: display::bad(),
            n: result.failures.len(),
        },
    ]);
    report::outcome(result.all_ok(), "lit OK", "lit FAILED");
}

/// The paths that are checked, for the standalone command's header.
pub fn paths_checked(paths: &Paths) -> Vec<String> {
    let root = repo_root(paths);
    let root: &Path = root.as_path();
    test_args(paths)
        .iter()
        .map(|p| {
            p.strip_prefix(root)
                .unwrap_or(p)
                .display()
                .to_string()
        })
        .collect()
}
