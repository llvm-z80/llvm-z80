//! The only place that decides how the runner's output looks.
//!
//! Suites describe *what* they found and never *how* to draw it. Every shape
//! the runner needs lives here, so adding a suite means picking one of them
//! rather than inventing a fifth summary format, which is how the output drifted
//! apart in the first place.
//!
//! Colour is not decided here either: printing goes through `anstream`, which
//! strips styling on a pipe or under `NO_COLOR`. See `display`.

use comfy_table::{ContentArrangement, presets};
use owo_colors::{OwoColorize, Style};

use crate::display;

/// The progress line currently drawing, if any.
///
/// Anything printed while a line is live has to go through it: indicatif owns
/// those screen rows, and a plain `println!` lands on top of the bar instead of
/// above it. `full` makes this unavoidable, since its phases print results
/// while the shared line is still moving.
static ACTIVE: std::sync::Mutex<Option<indicatif::ProgressBar>> =
    std::sync::Mutex::new(None);

/// Lines held back instead of printed, see `capture`.
static CAPTURED: std::sync::Mutex<Option<Vec<String>>> = std::sync::Mutex::new(None);

/// Hold every line until `release` instead of printing it.
///
/// `full` runs its phases together behind one progress line, and their running
/// commentary is worth less than a clean bar: three headings and a hundred
/// suite lines interleaved from three phases is not something anyone reads.
/// The lines are kept rather than dropped, so a failure can still show its
/// full detail afterwards.
pub fn capture() {
    *CAPTURED.lock().unwrap() = Some(Vec::new());
}

/// Stop holding lines back and return what was held.
pub fn release() -> Vec<String> {
    CAPTURED.lock().unwrap().take().unwrap_or_default()
}

/// Print one line, stepping around a live progress line if there is one.
pub fn print_line(text: &str) {
    if let Some(held) = CAPTURED.lock().unwrap().as_mut() {
        held.push(text.to_string());
        return;
    }
    let active = ACTIVE.lock().unwrap();
    match active.as_ref() {
        // Through indicatif's own handle, not ours: it has to clear the bar,
        // write above it and redraw, and it can only do that for writes it
        // performs itself. A `println!` from a separate stdout handle lands on
        // the bar's row no matter how the bar is suspended around it.
        Some(bar) => bar.println(text),
        None => anstream::println!("{text}"),
    }
}

/// `println!` that cannot collide with the progress line.
#[macro_export]
macro_rules! say {
    () => { $crate::report::print_line("") };
    ($($arg:tt)*) => { $crate::report::print_line(&format!($($arg)*)) };
}

/// A suite's title line, followed by a rule the same width.
pub fn heading(title: &str) {
    crate::say!("{}", title.style(display::heading()));
    crate::say!("{}", "=".repeat(title.chars().count()));
}

/// The `Target: z80` / `Build: ...` block some suites print before their
/// results. Keys are padded to a common width so the values line up.
pub fn fields(pairs: &[(&str, String)]) {
    let width = pairs.iter().map(|(k, _)| k.chars().count()).max().unwrap_or(0);
    for (key, value) in pairs {
        crate::say!(
            "{}{}  {value}",
            format_args!("{key}:").style(display::faint()),
            " ".repeat(width - key.chars().count())
        );
    }
}

/// One counted outcome in a summary line.
pub struct Count {
    pub label: String,
    pub style: Style,
    pub n: usize,
}

/// `CLANG 4   XFAIL 4   PASS 1866   SKIP 129`, in the order given.
pub fn tally(counts: &[Count]) {
    let cells: Vec<String> = counts
        .iter()
        .map(|c| format!("{}", format_args!("{} {}", c.label, c.n).style(c.style.bold())))
        .collect();
    crate::say!("  {}", cells.join("   "));
}

/// The verdict every suite ends on, so a caller can tell success from failure
/// without parsing counts.
pub fn outcome(ok: bool, ok_text: &str, bad_text: &str) {
    if ok {
        crate::say!("{}", ok_text.style(display::ok().bold()));
    } else {
        crate::say!("{}", bad_text.style(display::bad().bold()));
    }
}

pub enum Align {
    Left,
    Right,
}

/// A borderless aligned table. Column widths come from the content, so no call
/// site carries hardcoded `{:<24}` widths that go stale when a name grows.
///
/// Cells are plain text with one exception: the last column may carry styling,
/// because widening it cannot push anything out of line. Colouring an earlier
/// column would, since the layout counts escape bytes as width.
pub struct Table {
    inner: comfy_table::Table,
    aligns: Vec<comfy_table::CellAlignment>,
}

impl Table {
    pub fn new(headers: &[&str], aligns: &[Align]) -> Self {
        let aligns: Vec<comfy_table::CellAlignment> = aligns
            .iter()
            .map(|a| match a {
                Align::Left => comfy_table::CellAlignment::Left,
                Align::Right => comfy_table::CellAlignment::Right,
            })
            .collect();

        let mut inner = comfy_table::Table::new();
        inner.load_style(presets::NOTHING);
        // Never reflow: these tables are read next to each other across runs,
        // and a column that moves with the terminal width is hard to compare.
        inner.set_content_arrangement(ContentArrangement::Disabled);
        // Header cells stay plain text. comfy-table measures a cell by its
        // byte content, and it cannot see that an ANSI escape occupies no
        // columns unless its `custom_styling` feature is on, which drags in
        // crossterm. The header is styled a line at a time in `print` instead.
        inner.set_header(
            headers
                .iter()
                .zip(&aligns)
                .map(|(h, a)| comfy_table::Cell::new(*h).set_alignment(*a))
                .collect::<Vec<_>>(),
        );

        Table { inner, aligns }
    }

    pub fn row<I, S>(&mut self, cells: I) -> &mut Self
    where
        I: IntoIterator<Item = S>,
        S: Into<String>,
    {
        let row: Vec<comfy_table::Cell> = cells
            .into_iter()
            .enumerate()
            .map(|(i, c)| {
                let cell = comfy_table::Cell::new(c.into());
                match self.aligns.get(i) {
                    Some(a) => cell.set_alignment(*a),
                    None => cell,
                }
            })
            .collect();
        self.inner.add_row(row);
        self
    }

    pub fn print(&self) {
        for (i, line) in self.inner.to_string().lines().enumerate() {
            // comfy-table pads to the widest row; trailing blanks are noise in
            // a log.
            let line = line.trim_end();
            if i == 0 {
                crate::say!("{}", line.style(display::heading()));
            } else {
                crate::say!("{line}");
            }
        }
    }
}

/// `9,414,479,214`. Cycle counts are the main thing suites print and are
/// unreadable without separators.
pub fn thousands(n: u64) -> String {
    let s = n.to_string();
    let mut out = String::with_capacity(s.len() + s.len() / 3);
    for (i, c) in s.chars().enumerate() {
        if i > 0 && (s.len() - i).is_multiple_of(3) {
            out.push(',');
        }
        out.push(c);
    }
    out
}

/// A block of lines that update in place while parallel work runs.
///
/// The caller decides what each line says; this only owns the terminal
/// mechanics, which is the part that was hand-written twice with raw cursor
/// arithmetic. On anything that is not a terminal it degrades to printing each
/// line once as it settles, which is what a CI log wants.
pub struct Progress {
    bars: Vec<indicatif::ProgressBar>,
    /// Kept alive for as long as the bars are drawn; dropping it detaches the
    /// draw target. `None` when not attached to a terminal.
    multi: Option<indicatif::MultiProgress>,
    /// The final wording for each line, kept here rather than read back out of
    /// the bars: indicatif erases its drawing area when the MultiProgress is
    /// dropped, so what survives has to be reprinted from something we own.
    settled: Vec<Option<String>>,
}

impl Progress {
    /// A block that never redraws: each line is printed once, when it
    /// settles. This is what a log gets, and what `full` uses so its phases do
    /// not fight the single progress line it draws for the whole run.
    pub fn plain(title: &str, labels: &[String]) -> Self {
        heading(title);
        Progress { bars: Vec::new(), multi: None, settled: vec![None; labels.len()] }
    }

    pub fn new(title: &str, labels: &[String]) -> Self {
        heading(title);

        let settled = vec![None; labels.len()];
        if !display::is_tty() {
            return Progress { bars: Vec::new(), multi: None, settled };
        }

        let multi = indicatif::MultiProgress::new();
        // Draw where everything else goes. indicatif defaults to stderr, and
        // with the heading on stdout the two streams disagree about where the
        // block starts, so clearing it left the first line behind.
        multi.set_draw_target(indicatif::ProgressDrawTarget::stdout());
        // The heading goes out through the MultiProgress too. Printing it
        // beforehand through a separate stdout handle left indicatif's idea of
        // where its block starts two lines off, and clearing then stranded a
        // line on screen.
        // The message is the whole line: the suites already know how to word
        // their own progress, and a bar would say less than "52/52 skip=7".
        let style = indicatif::ProgressStyle::with_template("{msg}").expect("static template");
        let bars = labels
            .iter()
            .map(|label| {
                let bar = multi.add(indicatif::ProgressBar::new_spinner());
                bar.set_style(style.clone());
                bar.set_message(format!(
                    "{}",
                    format_args!("  \u{22ef} {label}  -").style(display::faint())
                ));
                bar
            })
            .collect();

        Progress { bars, multi: Some(multi), settled }
    }

    /// Replace line `i` while its work is still running.
    pub fn update(&self, i: usize, line: String) {
        if let Some(bar) = self.bars.get(i) {
            bar.set_message(line);
        }
    }

    /// Replace line `i` with its final form. Safe to call repeatedly; only the
    /// first call prints when there is no terminal to redraw.
    pub fn finish(&mut self, i: usize, line: String) {
        match self.settled.get(i) {
            Some(None) => {}
            _ => return,
        }
        match self.bars.get(i) {
            Some(bar) => bar.set_message(line.clone()),
            None => crate::say!("{line}"),
        }
        self.settled[i] = Some(line);
    }

    /// Take the live block down and leave the finished lines behind it.
    pub fn done(self) {
        let Some(multi) = self.multi else { return };
        for bar in &self.bars {
            bar.finish();
        }
        let _ = multi.clear();
        for line in self.settled.into_iter().flatten() {
            crate::say!("{line}");
        }
    }
}

/// A single line that updates in place, for work that is one long list rather
/// than a set of parallel suites.
///
/// Off a terminal it prints occasional plain lines instead, so a CI log gets
/// progress without control characters.
/// A one-line "N of M, so long elapsed" counter for a suite that works
/// through a single long list.
///
/// The elapsed time is rendered by indicatif on a steady tick rather than
/// being recomputed when a task finishes. Tasks do not complete at an even
/// rate here: the last few can each sit on the emulator for its whole budget,
/// and a clock that only advanced on completion looked stopped exactly when
/// the run was slowest and the reassurance mattered most.
///
/// Off a terminal it prints a line every so often instead, so a log gets
/// progress without control characters.
pub struct Line {
    bar: Option<indicatif::ProgressBar>,
    /// Completions, tracked separately only for the no-terminal path;
    /// otherwise the bar owns the count.
    done: std::sync::atomic::AtomicU64,
    total: u64,
    /// How many completions between printed lines when there is no terminal.
    log_every: u64,
}

impl Line {
    pub fn new(total: u64, noun: &str) -> Self {
        let done = std::sync::atomic::AtomicU64::new(0);
        if !display::is_tty() {
            return Line { bar: None, done, total, log_every: 250 };
        }

        let bar = indicatif::ProgressBar::new(total);
        bar.set_draw_target(indicatif::ProgressDrawTarget::stdout());
        // The bar carries its own colours through indicatif's template syntax;
        // only the text after it is wrapped, so the two do not fight over who
        // resets the colour. Safe to embed escapes here because the line stands
        // alone, with no columns to keep aligned.
        bar.set_style(
            indicatif::ProgressStyle::with_template(&format!(
                "  {{bar:24.green/black.bright}} {}",
                format_args!("{{pos}}/{{len}} {noun}  {{elapsed}} elapsed")
                    .style(display::muted())
            ))
            .expect("static template"),
        );
        // Redraw on a timer, not only when a task lands. Fast enough that the
        // seconds counter looks like a clock rather than a stalled display.
        bar.enable_steady_tick(std::time::Duration::from_millis(100));
        *ACTIVE.lock().unwrap() = Some(bar.clone());
        Line { bar: Some(bar), done, total, log_every: 250 }
    }

    /// Record one completed unit of work.
    pub fn inc(&self) {
        match &self.bar {
            Some(bar) => bar.inc(1),
            None => {
                let n = self.done.fetch_add(1, std::sync::atomic::Ordering::Relaxed) + 1;
                if n == self.total || n.is_multiple_of(self.log_every) {
                    crate::say!("  {n}/{}", self.total);
                }
            }
        }
    }

    /// Take the line back off the screen.
    pub fn clear(self) {
        if let Some(bar) = self.bar {
            *ACTIVE.lock().unwrap() = None;
            bar.finish_and_clear();
        }
    }
}
