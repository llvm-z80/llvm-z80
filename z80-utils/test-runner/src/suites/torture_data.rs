//! Manifest handling for the GCC C torture suite.
//!
//! The manifest lists only tests that a Z80 target *structurally* cannot pass:
//! a hosted libc, 64-bit `double`, a 32-bit `int`, a binary that does not fit in
//! the address space.  Backend bugs deliberately do NOT belong here.  They keep
//! running and keep failing until they are fixed, so an expected-failure entry
//! can never quietly hide a miscompile.
//!
//! Line format:
//!
//! ```text
//! <path-or-glob> [target] [opt] : skip=<reason>
//!
//! execute/*double*.c         : skip=softfloat-f64
//! execute/pr23135.c          : skip=int16
//! execute/990326-1.c   sm83  : skip=too-slow
//! ```
//!
//! An omitted target or opt qualifier means "every target" / "every opt level".

use std::path::Path;

use crate::config::{OptLevel, Target};

/// Reason codes the manifest accepts.  Closed on purpose: a new code is a new
/// claim about what the target cannot do, and that deserves review.
pub const REASONS: &[&str] = &[
    "libc",          // needs a hosted libc (printf, malloc, setjmp, signal, ...)
    "gcc",           // uses a GCC builtin clang does not implement
    "unsupported",   // uses a feature this target deliberately refuses
    "softfloat-f64", // needs 64-bit double; compiler-rt/z80 ships float only
    "callbr",        // uses asm goto; the GlobalISel translator has no lowering
    "int16",         // the test assumes a 32-bit int
    "size",          // does not fit in the 64 KB address space
    "nonportable",   // relies on host properties the target does not have
];

/// What the manifest says about a test.
#[derive(PartialEq)]
enum Action {
    /// Do not run it: the target structurally cannot pass.
    Skip,
    /// Run it, but report a failure as clang's rather than ours. Unlike a skip
    /// this keeps the test in the run, so the day clang fixes the bug the test
    /// simply starts passing and says so.
    Clang,
}

struct Rule {
    pattern: String,
    target: Option<Target>,
    opt: Option<OptLevel>,
    action: Action,
    reason: String,
}

#[derive(Default)]
pub struct Manifest {
    rules: Vec<Rule>,
}

impl Manifest {
    /// Load the manifest.  A missing file is not an error; it means nothing is
    /// skipped yet.
    pub fn load(path: &Path) -> Result<Manifest, String> {
        let text = match std::fs::read_to_string(path) {
            Ok(t) => t,
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => return Ok(Manifest::default()),
            Err(e) => return Err(format!("{}: {e}", path.display())),
        };
        let mut rules = Vec::new();
        for (i, raw) in text.lines().enumerate() {
            let line_no = i + 1;
            let line = raw.split('#').next().unwrap_or("").trim();
            if line.is_empty() {
                continue;
            }
            let at = |msg: String| format!("{}:{line_no}: {msg}", path.display());

            let (lhs, rhs) = line.rsplit_once(':').ok_or_else(|| at("missing ':'".into()))?;
            let rhs = rhs.trim();
            let (action, reason) = match rhs.split_once('=') {
                Some(("skip", r)) => (Action::Skip, r.trim().to_string()),
                Some(("clang", r)) => (Action::Clang, r.trim().to_string()),
                _ => return Err(at("expected skip=<reason> or clang=<note>".into())),
            };
            // A clang note is free text describing the upstream bug; only skip
            // reasons come from the closed set.
            if action == Action::Skip && !REASONS.contains(&reason.as_str()) {
                return Err(at(format!(
                    "unknown reason '{reason}' (known: {})",
                    REASONS.join(", ")
                )));
            }

            let mut tokens = lhs.split_whitespace();
            let pattern = tokens
                .next()
                .ok_or_else(|| at("missing pattern".into()))?
                .to_string();
            let (mut target, mut opt) = (None, None);
            for tok in tokens {
                if let Some(o) = OptLevel::parse(tok) {
                    opt = Some(o);
                } else if tok.eq_ignore_ascii_case("z80") {
                    target = Some(Target::Z80);
                } else if tok.eq_ignore_ascii_case("sm83") {
                    target = Some(Target::SM83);
                } else {
                    return Err(at(format!("unknown qualifier '{tok}'")));
                }
            }
            rules.push(Rule { pattern, target, opt, action, reason });
        }
        Ok(Manifest { rules })
    }

    fn lookup(&self, want: Action, name: &str, target: Target, opt: OptLevel) -> Option<&str> {
        self.rules
            .iter()
            .find(|r| {
                r.action == want
                    && r.target.is_none_or(|t| t == target)
                    && r.opt.is_none_or(|o| o == opt)
                    && glob_match(&r.pattern, name)
            })
            .map(|r| r.reason.as_str())
    }

    /// Why this test is skipped for the given configuration, if it is.
    pub fn skip_reason(&self, name: &str, target: Target, opt: OptLevel) -> Option<&str> {
        self.lookup(Action::Skip, name, target, opt)
    }

    /// The upstream clang bug this test trips over, if it is a known one.
    pub fn clang_note(&self, name: &str, target: Target, opt: OptLevel) -> Option<&str> {
        self.lookup(Action::Clang, name, target, opt)
    }

    pub fn is_empty(&self) -> bool {
        self.rules.is_empty()
    }
}

/// Match a manifest pattern against a test name.  `*` is the only
/// metacharacter and it spans `/` too, which is all the manifest needs
/// (`execute/*double*.c`).
fn glob_match(pattern: &str, name: &str) -> bool {
    let parts: Vec<&str> = pattern.split('*').collect();
    if parts.len() == 1 {
        return pattern == name;
    }
    let mut rest = match name.strip_prefix(parts[0]) {
        Some(r) => r,
        None => return false,
    };
    let last = parts.len() - 1;
    for (i, part) in parts.iter().enumerate().skip(1) {
        if i == last {
            return rest.len() >= part.len() && rest.ends_with(part);
        }
        if part.is_empty() {
            continue;
        }
        match rest.find(part) {
            Some(pos) => rest = &rest[pos + part.len()..],
            None => return false,
        }
    }
    true
}

#[cfg(test)]
mod tests {
    use super::glob_match;

    #[test]
    fn globs() {
        assert!(glob_match("execute/a.c", "execute/a.c"));
        assert!(!glob_match("execute/a.c", "execute/b.c"));
        assert!(glob_match("execute/*double*.c", "execute/pr-double-1.c"));
        assert!(glob_match("execute/*", "execute/ieee/x.c"));
        assert!(!glob_match("compile/*", "execute/a.c"));
        assert!(glob_match("*.c", "execute/a.c"));
        assert!(!glob_match("*.h", "execute/a.c"));
    }
}

/// DejaGnu directives embedded in the test sources.
///
/// GCC's harness runs `gcc.c-torture/execute` through `gcc-dg-runtest`, so every
/// `{ dg-... }` directive in a test comment applies. Two of them matter here:
///
/// * `dg-require-effective-target <feature>` is upstream's own statement of what
///   a target must provide to run the test. It is a far better source for the
///   skip list than guessing from undefined symbols: `int32plus` alone marks 70
///   tests that a 16-bit `int` can never pass.
/// * `dg-options` / `dg-additional-options` carry per-test compile flags. 380
///   tests ask for `-std=gnu89`, and 21 need `-fgnu89-inline` for their `inline`
///   functions to be emitted at all.
///
/// These are read from the sources at run time rather than copied into the
/// manifest: they are upstream's data, and mirroring them would just create a
/// second copy to keep in sync on every submodule bump.
pub struct Dg {
    /// Flags from dg-options / dg-additional-options, unfiltered.
    pub flags: Vec<String>,
    /// The first required feature this target does not have, with the manifest
    /// reason code that describes why. From dg-require-effective-target, which
    /// is about running the test, so it gates the execute tier only.
    pub missing: Option<(String, &'static str)>,
    /// The test carries a `dg-error`, so upstream expects the compile to be
    /// rejected. Failing to build it is the correct outcome, and building it
    /// cleanly is the bug.
    pub expects_error: bool,
}

/// Effective targets the Z80/SM83 backend does not provide, and the reason code
/// that explains each.
///
/// Deliberately conservative: a feature absent from this table is assumed to be
/// available, so the test runs and fails visibly rather than being skipped on a
/// guess. GCC gates some tests more tightly than we need (11 of 19
/// `label_values` tests pass here, 5 of 7 `untyped_assembly`), and skipping
/// those would hide working coverage.
const MISSING_TARGETS: &[(&str, &str)] = &[
    // 16-bit int, size_t and pointers.
    ("int32plus", "int16"),
    ("int32", "int16"),
    ("ptr32plus", "int16"),
    ("size32plus", "int16"),
    ("size20plus", "int16"),
    // compiler-rt/{z80,sm83} ships float only.
    ("double64plus", "softfloat-f64"),
    // GCC extensions clang does not implement.
    ("trampolines", "gcc"),
    ("return_address", "gcc"),
    ("nonlocal_goto", "gcc"),
    ("fopenmp", "gcc"),
    ("fgraphite", "gcc"),
    // Hosted environment.
    ("fileio", "libc"),
    ("c99_runtime", "libc"),
    ("pthread", "libc"),
    ("mmap", "libc"),
    // GCC itself runs these only when GCC_TEST_RUN_EXPENSIVE is set.
    ("run_expensive_tests", "too-slow"),
    // Decimal floating point, and another architecture entirely.
    ("dfp", "nonportable"),
    ("dfprt", "nonportable"),
    ("arm_arch_v5t_thumb_ok", "nonportable"),
];

/// Effective targets this one does have, consulted only for `dg-skip-if`,
/// which skips when the property IS present. Kept explicit and short: the
/// unknown case must not default to "present", or an exotic property would
/// silently skip a test that runs fine.
const PRESENT_TARGETS: &[&str] = &["freestanding"];

/// Does a `dg-skip-if` directive apply to this compilation?
///
/// The form is `dg-skip-if "reason" { selector } { include } { exclude }`,
/// where the option groups are optional. The selector is TCL and mostly names
/// other architectures (`bpf-*-*`, `avr-*-*`), which never match; only
/// `*-*-*`, an effective-target we have, and a negated one we lack do. The
/// include group narrows the skip to particular compile options, so a test
/// that upstream skips only under `-flto` must still run for us.
fn skip_if_applies(rest: &str, opt_flag: &str) -> bool {
    let mut groups = Vec::new();
    let mut depth = 0usize;
    let mut cur = String::new();
    for c in rest.chars() {
        match c {
            '{' => {
                depth += 1;
                if depth == 1 {
                    cur.clear();
                    continue;
                }
            }
            '}' => {
                if depth == 1 {
                    groups.push(std::mem::take(&mut cur));
                    depth = 0;
                    continue;
                }
                depth = depth.saturating_sub(1);
            }
            _ => {}
        }
        if depth == 1 {
            cur.push(c);
        }
    }
    let selector = match groups.first() {
        Some(g) => g.clone(),
        None => return false,
    };
    if !selector_matches(&selector) {
        return false;
    }

    // Options may also appear unbraced, as in `{ *-*-* } "-O1" ""`.
    let tail = match rest.split_once('}') {
        Some((_, t)) => t,
        None => "",
    };
    let quoted: Vec<String> = if groups.len() > 1 {
        groups[1].split_whitespace().map(|t| t.trim_matches('"').to_string()).collect()
    } else {
        tail.split_whitespace()
            .filter(|t| t.starts_with('"'))
            .map(|t| t.trim_matches('"').to_string())
            .collect()
    };
    let include: Vec<&String> = quoted.iter().filter(|o| !o.is_empty()).collect();
    if include.is_empty() {
        return true; // no option narrowing
    }
    include.iter().any(|o| o.as_str() == opt_flag)
}

fn selector_matches(selector: &str) -> bool {
    let tokens: Vec<&str> = selector
        .split(|c: char| c.is_whitespace() || c == '{' || c == '}')
        .map(|t| t.trim_matches('"'))
        .filter(|t| !t.is_empty())
        .collect();
    let mut negate = false;
    for tok in tokens {
        if tok == "!" {
            negate = true;
            continue;
        }
        let hit = if tok == "*-*-*" {
            !negate
        } else if tok.contains('-') {
            false // another architecture's triple
        } else if negate {
            MISSING_TARGETS.iter().any(|(f, _)| *f == tok)
        } else {
            PRESENT_TARGETS.contains(&tok)
        };
        if hit {
            return true;
        }
        negate = false;
    }
    false
}

/// Whether `dg-skip-if` tells the harness not to build this test at the given
/// optimization level. Unlike the rest of `Dg` this is level dependent: a skip
/// narrowed to `{ "-flto" }` or `{ "-O1" }` applies only there.
pub fn dg_skip_if(source: &str, opt_flag: &str) -> Option<(String, &'static str)> {
    for line in source.lines() {
        let Some(rest) = line.split("dg-skip-if").nth(1) else {
            continue;
        };
        if !skip_if_applies(rest, opt_flag) {
            continue;
        }
        let reason = rest.split('"').nth(1).unwrap_or("").trim();
        let why = if reason.is_empty() { "dg-skip-if" } else { reason };
        return Some((format!("dg-skip-if: {why}"), "unsupported"));
    }
    None
}

/// Per-test flags from the DejaGnu `.x` file beside a test.
///
/// These carry options the test cannot express in its own source, and without
/// them the test measures something else: `builtins/abs-1` asks for
/// `-fno-builtin-abs` so that `labs` folds and `abs` does not, and its
/// companion library aborts unless exactly that happens.
pub fn dot_x_flags(test: &std::path::Path) -> Vec<String> {
    let x = test.with_extension("x");
    let text = match std::fs::read_to_string(&x) {
        Ok(t) => t,
        Err(_) => return Vec::new(),
    };
    let mut flags = Vec::new();
    for line in text.lines() {
        // Only unindented lines: an indented one sits inside a TCL conditional
        // this parser does not evaluate.
        if line.starts_with(char::is_whitespace) {
            continue;
        }
        let rest = match line.strip_prefix("set additional_flags") {
            Some(r) => r,
            None => match line.strip_prefix("lappend additional_flags") {
                Some(r) => r,
                None => continue,
            },
        };
        for tok in rest.split_whitespace() {
            let tok = tok.trim_matches('"');
            if !tok.is_empty() {
                flags.push(tok.to_string());
            }
        }
    }
    flags
}

pub fn parse_dg(source: &str) -> Dg {
    let mut flags = Vec::new();
    let mut missing = None;
    let mut expects_error = false;

    for line in source.lines() {
        if !line.contains("dg-") {
            continue;
        }
        if line.contains("dg-error") {
            expects_error = true;
        }
        if let Some(rest) = line.split("dg-require-effective-target").nth(1) {
            let feature = rest
                .trim_start()
                .split(|c: char| !(c.is_ascii_alphanumeric() || c == '_'))
                .next()
                .unwrap_or("");
            if missing.is_none() {
                if let Some((_, reason)) = MISSING_TARGETS.iter().find(|(f, _)| *f == feature) {
                    missing = Some((feature.to_string(), *reason));
                }
            }
            continue;
        }
        // Both spellings are additive for us; GCC distinguishes them only
        // because dg-options replaces the torture flags it manages itself.
        for key in ["dg-additional-options", "dg-options"] {
            if let Some(rest) = line.split(key).nth(1) {
                if let Some(body) = rest.split('"').nth(1) {
                    flags.extend(body.split_whitespace().map(str::to_string));
                }
                break;
            }
        }
    }
    Dg { flags, missing, expects_error }
}
