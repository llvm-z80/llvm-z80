use std::fmt;
use std::path::Path;
use std::sync::atomic::{AtomicU64, Ordering};

use crate::config::{OptLevel, Target};

#[derive(Clone, Debug)]
pub enum TestOutcome {
    Pass { reg_value: String },
    Fail { got: String, expected: String },
    Fatal { reason: String },
    Skip { reason: String },
}

#[derive(Clone, Debug)]
pub struct TestResult {
    pub tag: String,
    pub outcome: TestOutcome,
}

impl TestResult {
    pub fn pass(tag: impl Into<String>, reg_value: impl Into<String>) -> Self {
        TestResult {
            tag: tag.into(),
            outcome: TestOutcome::Pass { reg_value: reg_value.into() },
        }
    }

    pub fn fail(tag: impl Into<String>, got: impl Into<String>, expected: impl Into<String>) -> Self {
        TestResult {
            tag: tag.into(),
            outcome: TestOutcome::Fail { got: got.into(), expected: expected.into() },
        }
    }

    pub fn fatal(tag: impl Into<String>, reason: impl Into<String>) -> Self {
        TestResult {
            tag: tag.into(),
            outcome: TestOutcome::Fatal { reason: reason.into() },
        }
    }

    pub fn skip(tag: impl Into<String>, reason: impl Into<String>) -> Self {
        TestResult {
            tag: tag.into(),
            outcome: TestOutcome::Skip { reason: reason.into() },
        }
    }

    pub fn is_pass(&self) -> bool {
        matches!(self.outcome, TestOutcome::Pass { .. })
    }
}

/// Run a built program and judge it against the `expect` directive in its
/// source. Every suite that checks a program's return value does exactly this,
/// so the four copies of it live here.
pub fn judge(
    tag: &str,
    bin: &Path,
    target: crate::config::Target,
    halt_addr: &str,
    result_addr: u32,
    dump: &Path,
    source: &str,
) -> TestResult {
    let got = match crate::emulator::run_program(
        bin, target, halt_addr, result_addr, dump, target.emu_timeout_secs())
    {
        Ok(r) => r.value,
        Err(e) => return TestResult::fatal(tag, e),
    };
    let expected = crate::emulator::parse_expected(source);
    match crate::emulator::check_result(&got, &expected) {
        Ok(()) => TestResult::pass(tag, format!("0x{got}")),
        Err((got_padded, exp_padded)) => {
            TestResult::fail(tag, format!("0x{got_padded}"), format!("0x{exp_padded}"))
        }
    }
}

/// Callback invoked after each test completes.
/// Receives the TestResult and the target's register name (e.g. "DE").
pub type OnResult = Box<dyn FnMut(&TestResult, &str) + Send>;

#[derive(Clone, Default)]
pub struct SuiteResult {
    pub pass: u32,
    pub fail: u32,
    pub fatal: u32,
    pub skip: u32,
    pub total: u32,
    pub results: Vec<TestResult>,
}

impl SuiteResult {
    pub fn add(&mut self, result: TestResult, on_result: &mut OnResult, reg_name: &str) {
        self.total += 1;
        match &result.outcome {
            TestOutcome::Pass { .. } => self.pass += 1,
            TestOutcome::Fail { .. } => self.fail += 1,
            TestOutcome::Fatal { .. } => self.fatal += 1,
            TestOutcome::Skip { .. } => self.skip += 1,
        }
        on_result(&result, reg_name);
        self.results.push(result);
    }

    /// Merge counters from another SuiteResult (results already reported via on_result).
    pub fn merge(&mut self, other: SuiteResult) {
        self.pass += other.pass;
        self.fail += other.fail;
        self.fatal += other.fatal;
        self.skip += other.skip;
        self.total += other.total;
    }

    pub fn all_ok(&self) -> bool {
        self.fail == 0 && self.fatal == 0
    }
}

impl fmt::Display for SuiteResult {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "Total: {}  Pass: {}  Fail: {}  Fatal: {}  Skip: {}",
            self.total, self.pass, self.fail, self.fatal, self.skip
        )
    }
}

/// Expand opt filter into list of opt levels.
pub fn expand_opt_levels(filter: &str) -> Vec<OptLevel> {
    match filter {
        "all" => vec![OptLevel::O0, OptLevel::O1, OptLevel::O2, OptLevel::O3, OptLevel::Os, OptLevel::Oz],
        other => OptLevel::parse(other).map(|o| vec![o]).unwrap_or_default(),
    }
}

/// Expand opt levels for LLC (no Os/Oz).
pub fn expand_llc_opt_levels(filter: &str) -> Vec<OptLevel> {
    match filter {
        "all" => vec![OptLevel::O0, OptLevel::O1],
        other => OptLevel::parse(other).map(|o| vec![o]).unwrap_or_default(),
    }
}

/// Generate a unique temporary directory name for the current thread.
/// Uses PID + monotonic counter to avoid collisions across threads.
pub fn unique_tmp_dir(base: &Path) -> std::path::PathBuf {
    static COUNTER: AtomicU64 = AtomicU64::new(0);
    let id = COUNTER.fetch_add(1, Ordering::Relaxed);
    base.join(format!("tmp_{}_{}", std::process::id(), id))
}

/// Remove a temporary directory and all its contents. Errors are silently ignored.
pub fn remove_tmp_dir(dir: &Path) {
    let _ = std::fs::remove_dir_all(dir);
}

/// Clean up leftover `tmp_*` directories from previous runs.
///
/// Directories belonging to a process that is still alive are left alone.
/// `unique_tmp_dir` stamps the owning pid into the name precisely so this can
/// tell them apart: a second runner started while one is mid-run would
/// otherwise delete the first one's working files, and the first would fail
/// with its emulator RAM dump or object files having vanished underneath it.
pub fn cleanup_old_tmp_dirs(dir: &Path) {
    let entries = match std::fs::read_dir(dir) {
        Ok(e) => e,
        Err(_) => return,
    };
    for entry in entries.flatten() {
        let name = entry.file_name();
        let name = name.to_string_lossy();
        if name.starts_with("tmp_") && entry.path().is_dir() && !owner_is_running(&name) {
            let _ = std::fs::remove_dir_all(entry.path());
        }
    }
}

/// Whether the pid embedded in a `tmp_<pid>_<n>` name is still running. Falls
/// back to "not running" wherever /proc is unavailable, which restores the
/// unconditional cleanup this replaced.
fn owner_is_running(name: &str) -> bool {
    let pid = match name.strip_prefix("tmp_").and_then(|r| r.split('_').next()) {
        Some(p) if !p.is_empty() && p.bytes().all(|b| b.is_ascii_digit()) => p,
        _ => return false,
    };
    Path::new("/proc").join(pid).is_dir()
}

/// Extract a meaningful error message from compiler/linker stderr output.
pub fn extract_error(stderr: &str) -> String {
    for line in stderr.lines() {
        if line.contains("ran out of registers")
            || line.contains("LLVM ERROR")
            || line.contains("Segmentation fault")
            || line.contains("error:")
        {
            return line.trim().to_string();
        }
    }
    "error".to_string()
}

/// Parse SKIP-IF directives from C source files.
/// Format: `/* SKIP-IF: <conditions> */`
/// Conditions: flags (-ffast-math, -fomit-frame-pointer), target (sm83, z80),
/// and/or opt levels (O0, O1, O2, O3, Os, Oz).
pub fn check_skip_c(
    source: &str,
    target: Target,
    active_flags: &[&str],
    opt_tag: &str,
) -> Option<String> {
    let opt_levels = ["O0", "O1", "O2", "O3", "Os", "Oz"];

    for line in source.lines() {
        let lower = line.to_lowercase();
        if let Some(pos) = lower.find("skip-if:") {
            let after = &line[pos + "skip-if:".len()..];
            let conditions = after
                .split("*/")
                .next()
                .unwrap_or("")
                .trim();

            let tokens: Vec<&str> = conditions.split_whitespace().collect();
            let mut flag = None;
            let mut target_filter = None;

            for token in &tokens {
                // Check opt level match (e.g. "O0")
                if opt_levels.iter().any(|o| o.eq_ignore_ascii_case(token)) {
                    if token.eq_ignore_ascii_case(opt_tag) {
                        return Some(conditions.to_string());
                    }
                    continue;
                }
                // `-flag` and `+feature` (e.g. +static-stack) both name a
                // compiler flag matched against active_flags; anything else is
                // a target triple filter.
                if token.starts_with('-') || token.starts_with('+') {
                    flag = Some(*token);
                } else {
                    target_filter = Some(*token);
                }
            }

            // Case 1: target-only skip
            if flag.is_none() {
                if let Some(tf) = target_filter {
                    if tf.eq_ignore_ascii_case(target.triple()) {
                        return Some(conditions.to_string());
                    }
                }
                continue;
            }

            // Case 2: flag-based skip
            if let Some(f) = flag {
                if active_flags.iter().any(|af| af.eq_ignore_ascii_case(f)) {
                    if let Some(tf) = target_filter {
                        if tf.eq_ignore_ascii_case(target.triple()) {
                            return Some(conditions.to_string());
                        }
                    } else {
                        return Some(conditions.to_string());
                    }
                }
            }
        }
    }
    None
}

/// Parse EXTRA-FLAGS directive from C source files.
/// Format: `/* EXTRA-FLAGS: -flag1 -flag2 */`
/// Returns additional compiler flags specified in the test file.
pub fn parse_extra_flags_c(source: &str) -> Vec<String> {
    let mut flags = Vec::new();
    for line in source.lines() {
        let lower = line.to_lowercase();
        if let Some(pos) = lower.find("extra-flags:") {
            let after = &line[pos + "extra-flags:".len()..];
            let content = after.split("*/").next().unwrap_or("").trim();
            for token in content.split_whitespace() {
                flags.push(token.to_string());
            }
        }
    }
    flags
}

/// Parse SKIP-IF directives from LLVM IR source files.
/// Format: `; SKIP-IF: <token>`
/// Token can be a target name or opt level.
pub fn check_skip_ll(
    source: &str,
    target: Target,
    opt: OptLevel,
) -> Option<String> {
    for line in source.lines() {
        let lower = line.to_lowercase();
        if let Some(pos) = lower.find("skip-if:") {
            let after = &line[pos + "skip-if:".len()..];
            let conditions = after.trim();
            for token in conditions.split_whitespace() {
                if token.eq_ignore_ascii_case(target.triple())
                    || token.eq_ignore_ascii_case(opt.clang_flag())
                {
                    return Some(token.to_string());
                }
            }
        }
    }
    None
}

/// Discover test files matching a glob pattern in a directory.
pub fn discover_tests(dir: &Path, prefix: &str, ext: &str) -> Vec<std::path::PathBuf> {
    let pattern = format!("{prefix}*.{ext}");
    let mut tests: Vec<_> = std::fs::read_dir(dir)
        .into_iter()
        .flatten()
        .filter_map(|e| e.ok())
        .map(|e| e.path())
        .filter(|p| {
            p.file_name()
                .and_then(|n| n.to_str())
                .is_some_and(|n| {
                    n.starts_with(prefix) && n.ends_with(&format!(".{ext}"))
                        && !n.contains("_clang.") && !n.contains("_sdcc.")
                })
        })
        .collect();
    tests.sort();
    let _ = pattern; // suppress unused
    tests
}

/// Run a command with timeout. Returns (exit_code, stdout="", stderr).
///
/// stdout is sent to /dev/null (callers only use stderr for error messages).
/// stderr is drained in a background thread to prevent pipe deadlock.
pub fn run_cmd_timeout(
    cmd: &mut std::process::Command,
    timeout_secs: u64,
) -> Result<(i32, String, String), String> {
    use std::io::Read;
    use std::time::{Duration, Instant};

    cmd.stdout(std::process::Stdio::null());
    cmd.stderr(std::process::Stdio::piped());

    let mut child = cmd.spawn().map_err(|e| e.to_string())?;

    // Drain stderr in a background thread to prevent pipe deadlock.
    let mut stderr_pipe = child.stderr.take().unwrap();
    let stderr_reader = std::thread::spawn(move || {
        let mut buf = String::new();
        let _ = stderr_pipe.read_to_string(&mut buf);
        buf
    });

    let start = Instant::now();
    let timeout = Duration::from_secs(timeout_secs);

    loop {
        match child.try_wait() {
            Ok(Some(status)) => {
                let stderr = stderr_reader.join().unwrap_or_default();
                // A process killed by a signal has no exit code. Report the
                // signal negated so callers can tell a crash from a compiler
                // that merely rejected its input; a deep enough nesting makes
                // clang's parser overflow the stack and die without printing
                // anything at all.
                let code = match status.code() {
                    Some(c) => c,
                    None => -std::os::unix::process::ExitStatusExt::signal(&status).unwrap_or(1),
                };
                return Ok((code, String::new(), stderr));
            }
            Ok(None) => {
                if start.elapsed() > timeout {
                    let _ = child.kill();
                    let _ = child.wait();
                    return Err(format!("timeout/{timeout_secs}s"));
                }
                std::thread::sleep(Duration::from_millis(5));
            }
            Err(e) => return Err(e.to_string()),
        }
    }
}

#[cfg(test)]
mod cleanup_tests {
    use super::*;

    #[test]
    fn keeps_dirs_owned_by_a_live_process() {
        let base = std::env::temp_dir().join(format!("z80-cleanup-test-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&base);
        let live = base.join(format!("tmp_{}_0", std::process::id()));
        // A pid this high will not be in use.
        let dead = base.join("tmp_4000000_0");
        let junk = base.join("tmp_notapid_0");
        for d in [&live, &dead, &junk] {
            std::fs::create_dir_all(d).unwrap();
        }

        cleanup_old_tmp_dirs(&base);

        assert!(live.is_dir(), "a running process's dir must survive");
        assert!(!dead.is_dir(), "a dead process's dir must be removed");
        assert!(!junk.is_dir(), "an unparsable name must still be removed");
        let _ = std::fs::remove_dir_all(&base);
    }
}
