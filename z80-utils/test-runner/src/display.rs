//! Shared terminal output.
//!
//! Colour is never decided here. Everything prints through `anstream`, which
//! drops the styling itself when the stream is not a terminal, when `NO_COLOR`
//! is set, or when `CLICOLOR=0` asks for it, and keeps it when `CLICOLOR_FORCE`
//! demands it. Call sites therefore style unconditionally: the `if tty { ... }
//! else { ... }` pairs this module used to carry are gone, and with them the
//! chance of the two branches drifting apart.

use std::io::{IsTerminal, Write};

use owo_colors::{OwoColorize, Style};

use crate::suite::TestOutcome;

/// Semantic palette. Suites name an outcome, not a colour, so the same kind of
/// result looks the same everywhere.
pub fn ok() -> Style {
    Style::new().green()
}
pub fn bad() -> Style {
    Style::new().red()
}
pub fn warn() -> Style {
    Style::new().yellow()
}
pub fn crash() -> Style {
    Style::new().magenta()
}
pub fn upstream() -> Style {
    Style::new().bright_magenta()
}
pub fn info() -> Style {
    Style::new().blue()
}
pub fn muted() -> Style {
    // xterm 245 rather than bright black: the ANSI bright-black many terminals
    // render is nearly the background, which made progress text hard to read.
    Style::new().fg::<owo_colors::colors::xterm::LightGray>()
}
/// Colour for a toolchain's name, so the same compiler reads the same way in
/// every table and summary rather than each suite picking again.
pub fn compiler(name: &str) -> Style {
    match name.to_ascii_lowercase().as_str() {
        "clang" => Style::new().green(),
        "sdcc" => Style::new().yellow(),
        _ => Style::new(),
    }
}

pub fn heading() -> Style {
    Style::new().bold()
}
pub fn faint() -> Style {
    Style::new().dimmed()
}

/// Whether stdout is a terminal.
///
/// This answers "may I redraw a line in place", not "may I use colour";
/// anstream owns the latter. Repainting a progress line on a pipe would just
/// litter the log with control characters, so callers that redraw still ask.
pub fn is_tty() -> bool {
    std::io::stdout().is_terminal()
}

/// Print a single test result line (for individual suite mode).
pub fn print_test_result(outcome: &TestOutcome, tag: &str, reg_name: &str) {
    match outcome {
        TestOutcome::Pass { reg_value } => {
            crate::say!("  {}  {tag}  ({reg_name}={reg_value})", "PASS".style(ok()));
        }
        TestOutcome::Fail { got, expected } => {
            crate::say!(
                "  {}  {tag}  ({reg_name}={got}, expected {expected})",
                "FAIL".style(bad())
            );
        }
        TestOutcome::Fatal { reason } => {
            crate::say!("  {} {tag}  ({reason})", "FATAL".style(crash()));
        }
        TestOutcome::Skip { reason } => {
            crate::say!("  {}  {tag}  ({reason})", "SKIP".style(warn()));
        }
    }
    let _ = std::io::stdout().flush();
}

pub fn print_summary(total: u32, pass: u32, fail: u32, fatal: u32, skip: u32, all_ok: bool) {
    crate::say!(
        "  {}",
        format_args!("Total: {total}  Pass: {pass}  Fail: {fail}  Fatal: {fatal}  Skip: {skip}")
            .style(heading())
    );
    if all_ok {
        crate::say!("  {}", "ALL PASS".style(ok().bold()));
    } else {
        crate::say!("  {}", "SOME FAILURES".style(bad().bold()));
    }
    let _ = std::io::stdout().flush();
}
