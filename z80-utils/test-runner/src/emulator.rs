use std::io::{BufRead, Read};
use std::path::Path;
use std::process::{Command, Stdio};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

use crate::config::Target;

/// Find the `_halt` symbol address from an SDCC linker map file (.map).
/// Map entries are separated by `|`, each formatted as "  ADDR  SYMBOL  ".
pub fn halt_addr_from_map(map_file: &Path) -> Option<String> {
    let content = std::fs::read_to_string(map_file).ok()?;
    for line in content.lines() {
        for entry in line.split('|') {
            let parts: Vec<&str> = entry.split_whitespace().collect();
            if parts.len() >= 2 && parts[1] == "_halt" {
                if let Ok(addr) = u32::from_str_radix(parts[0], 16) {
                    return Some(format!("0x{:04X}", addr));
                }
            }
        }
    }
    None
}

/// Find the `_halt` symbol address from an ELF using llvm-nm.
pub fn halt_addr_from_elf(llvm_nm: &Path, elf: &Path) -> Option<String> {
    let output = std::process::Command::new(llvm_nm)
        .arg(elf)
        .output()
        .ok()?;
    if !output.status.success() { return None; }
    let stdout = String::from_utf8_lossy(&output.stdout);
    // llvm-nm output: "0000001c T _halt"
    for line in stdout.lines() {
        let parts: Vec<&str> = line.split_whitespace().collect();
        if parts.len() >= 3 && parts[2] == "_halt" {
            if let Ok(addr) = u32::from_str_radix(parts[0], 16) {
                return Some(format!("0x{:04X}", addr));
            }
        }
    }
    None
}

/// Run makebin to convert .ihx to .bin.
pub fn makebin(ihx: &Path, bin: &Path) -> Result<(), String> {
    let status = Command::new("makebin")
        .args([ihx.as_os_str(), bin.as_os_str()])
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status()
        .map_err(|e| format!("makebin: {e}"))?;
    if status.success() {
        Ok(())
    } else {
        Err("makebin failed".to_string())
    }
}

/// Convert ELF to flat binary using llvm-objcopy.
pub fn elf_to_bin(objcopy: &Path, elf: &Path, bin: &Path) -> Result<(), String> {
    let output = Command::new(objcopy)
        .args(["-O", "binary"])
        .arg(elf.as_os_str())
        .arg(bin.as_os_str())
        .output()
        .map_err(|e| format!("llvm-objcopy: {e}"))?;
    if output.status.success() {
        Ok(())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("llvm-objcopy failed: {stderr}"))
    }
}

/// Run z88dk-ticks emulator and extract the result register value.
///
/// `-trace` generates huge stdout (every instruction). We drain it in a
/// background thread using `BufReader::read_line` with a large internal
/// buffer, scanning each line for the register pattern.
/// Only the last matched value is kept.
pub fn emulate(bin: &Path, target: Target, halt_addr: &str) -> Result<String, String> {
    let timeout = Duration::from_secs(target.emu_timeout_secs());

    let mut cmd = Command::new("z88dk-ticks");
    for flag in target.emu_flags() {
        cmd.arg(flag);
    }
    cmd.args(["-trace", "-end", halt_addr]);
    cmd.arg(bin);
    cmd.stdout(Stdio::piped());
    cmd.stderr(Stdio::null());

    let mut child = cmd.spawn().map_err(|e| format!("z88dk-ticks: {e}"))?;

    // Take stdout pipe — drain in a reader thread to prevent pipe deadlock.
    let stdout = child.stdout.take().unwrap();
    let needle = format!("{}=", target.reg_grep()); // "de=" or "bc="
    let killed = Arc::new(AtomicBool::new(false));
    let killed2 = Arc::clone(&killed);

    let reader = std::thread::spawn(move || {
        drain_for_register(stdout, &needle, &killed2)
    });

    // Timeout loop
    let start = std::time::Instant::now();
    loop {
        match child.try_wait() {
            Ok(Some(_)) => break,
            Ok(None) => {
                if start.elapsed() > timeout {
                    killed.store(true, Ordering::Relaxed);
                    let _ = child.kill();
                    let _ = child.wait();
                    return Err(format!("emulator timeout/{}s", timeout.as_secs()));
                }
                std::thread::sleep(Duration::from_millis(5));
            }
            Err(e) => return Err(format!("wait: {e}")),
        }
    }

    let value = reader.join().map_err(|_| "reader thread panicked".to_string())?;
    match value {
        Some(v) => Ok(v),
        None => Err(format!("no register value in emulator output")),
    }
}

/// Drain emulator stdout line by line, extracting the last register value.
/// Uses BufReader with a 512 KB internal buffer. Reuses a single String
/// buffer for read_line to avoid per-line heap allocation.
fn drain_for_register(
    reader: impl Read,
    needle: &str,
    killed: &AtomicBool,
) -> Option<String> {
    let mut buf_reader = std::io::BufReader::with_capacity(512 * 1024, reader);
    let mut line_buf = String::with_capacity(256);
    let mut last_value: Option<String> = None;
    let needle_lower = needle.to_lowercase();

    loop {
        if killed.load(Ordering::Relaxed) {
            break;
        }
        line_buf.clear();
        match buf_reader.read_line(&mut line_buf) {
            Ok(0) => break, // EOF
            Ok(_) => {}
            Err(_) => break,
        }

        let lower = line_buf.to_lowercase();
        if let Some(pos) = lower.find(&needle_lower) {
            let hex_start = pos + needle_lower.len();
            let hex: String = lower[hex_start..]
                .chars()
                .take_while(|c| c.is_ascii_hexdigit())
                .collect();
            if !hex.is_empty() {
                last_value = Some(hex.to_uppercase());
            }
        }
    }

    last_value
}

/// Parse expected value from test source file.
/// Looks for "expect 0xXXXX" comment, defaults to 0x000F.
pub fn parse_expected(source: &str) -> String {
    for line in source.lines() {
        let lower = line.to_lowercase();
        if let Some(pos) = lower.find("expect 0x") {
            let hex_start = pos + "expect 0x".len();
            let hex: String = lower[hex_start..]
                .chars()
                .take_while(|c| c.is_ascii_hexdigit())
                .collect();
            if !hex.is_empty() {
                return hex.to_uppercase();
            }
        }
    }
    "000F".to_string()
}

/// Check emulator result against expected value.
pub fn check_result(
    got: &str,
    expected: &str,
) -> Result<(), (String, String)> {
    let got_upper = got.to_uppercase();
    let exp_upper = expected.to_uppercase();
    // Pad to same length for comparison
    let max_len = got_upper.len().max(exp_upper.len());
    let got_padded = format!("{:0>width$}", got_upper, width = max_len);
    let exp_padded = format!("{:0>width$}", exp_upper, width = max_len);

    if got_padded == exp_padded {
        Ok(())
    } else {
        Err((got_padded, exp_padded))
    }
}
