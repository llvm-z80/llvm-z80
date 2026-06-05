use std::io::{self, Write};

use crate::suite::TestOutcome;

const GREEN: &str = "\x1b[32m";
const RED: &str = "\x1b[31m";
const YELLOW: &str = "\x1b[33m";
const MAGENTA: &str = "\x1b[35m";
const BOLD: &str = "\x1b[1m";
const RESET: &str = "\x1b[0m";

pub fn is_tty() -> bool {
    unsafe { libc_isatty(1) != 0 }
}

unsafe extern "C" {
    #[link_name = "isatty"]
    fn libc_isatty(fd: i32) -> i32;
}

/// Print a single test result line (for individual suite mode).
pub fn print_test_result(outcome: &TestOutcome, tag: &str, reg_name: &str) {
    let tty = is_tty();
    match outcome {
        TestOutcome::Pass { reg_value } => {
            if tty {
                println!("  {GREEN}PASS{RESET}  {tag}  ({reg_name}={reg_value})");
            } else {
                println!("  PASS  {tag}  ({reg_name}={reg_value})");
            }
        }
        TestOutcome::Fail { got, expected } => {
            if tty {
                println!("  {RED}FAIL{RESET}  {tag}  ({reg_name}={got}, expected {expected})");
            } else {
                println!("  FAIL  {tag}  ({reg_name}={got}, expected {expected})");
            }
        }
        TestOutcome::Fatal { reason } => {
            if tty {
                println!("  {MAGENTA}FATAL{RESET} {tag}  ({reason})");
            } else {
                println!("  FATAL {tag}  ({reason})");
            }
        }
        TestOutcome::Skip { reason } => {
            if tty {
                println!("  {YELLOW}SKIP{RESET}  {tag}  ({reason})");
            } else {
                println!("  SKIP  {tag}  ({reason})");
            }
        }
    }
    let _ = io::stdout().flush();
}

pub fn print_summary(total: u32, pass: u32, fail: u32, fatal: u32, skip: u32, all_ok: bool) {
    let tty = is_tty();
    if tty {
        println!("  {BOLD}Total: {total}  Pass: {pass}  Fail: {fail}  Fatal: {fatal}  Skip: {skip}{RESET}");
        if all_ok {
            println!("  {GREEN}{BOLD}ALL PASS{RESET}");
        } else {
            println!("  {RED}{BOLD}SOME FAILURES{RESET}");
        }
    } else {
        println!("  Total: {total}  Pass: {pass}  Fail: {fail}  Fatal: {fatal}  Skip: {skip}");
        if all_ok {
            println!("  ALL PASS");
        } else {
            println!("  SOME FAILURES");
        }
    }
    let _ = io::stdout().flush();
}
