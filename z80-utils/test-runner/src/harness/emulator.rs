use std::path::Path;
use std::process::{Command, Stdio};
use std::time::Duration;

use crate::config::Target;

/// Find a symbol's address in an SDCC linker map file (.map).
/// Map entries are separated by `|`, each formatted as "  ADDR  SYMBOL  ".
pub fn symbol_addr_from_map(map_file: &Path, name: &str) -> Option<u32> {
    let content = std::fs::read_to_string(map_file).ok()?;
    for line in content.lines() {
        for entry in line.split('|') {
            let parts: Vec<&str> = entry.split_whitespace().collect();
            // The map truncates names to nine characters, so a longer symbol
            // only ever appears as a prefix of itself.
            if parts.len() >= 2 && (parts[1] == name || name.starts_with(parts[1]) && parts[1].len() >= 9) {
                return u32::from_str_radix(parts[0], 16).ok();
            }
        }
    }
    None
}

/// Find the `_halt` symbol address from an SDCC linker map file (.map).
pub fn halt_addr_from_map(map_file: &Path) -> Option<String> {
    symbol_addr_from_map(map_file, "_halt").map(|a| format!("0x{a:04X}"))
}

/// Find a symbol's address in an ELF using llvm-nm.
pub fn symbol_addr_from_elf(llvm_nm: &Path, elf: &Path, name: &str) -> Option<u32> {
    let output = std::process::Command::new(llvm_nm).arg(elf).output().ok()?;
    if !output.status.success() {
        return None;
    }
    let stdout = String::from_utf8_lossy(&output.stdout);
    // llvm-nm output: "0000001c T _halt"
    for line in stdout.lines() {
        let parts: Vec<&str> = line.split_whitespace().collect();
        if parts.len() >= 3 && parts[2] == name {
            return u32::from_str_radix(parts[0], 16).ok();
        }
    }
    None
}

/// Find the `_halt` symbol address from an ELF using llvm-nm.
pub fn halt_addr_from_elf(llvm_nm: &Path, elf: &Path) -> Option<String> {
    symbol_addr_from_elf(llvm_nm, elf, "_halt").map(|a| format!("0x{a:04X}"))
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

/// Cycle budget handed to z88dk-ticks.
///
/// Reaching it means the program never got to its halt address. This has to be
/// checked: at its own default limit the emulator exits *successfully*, so a
/// spinning program would otherwise look like a clean run whose `_exitcode` is
/// still the zero the .bss loop left there, and report as a pass.
///
/// The budget a caller passes is in cycles, not seconds. Cycles are a property
/// of the program: the same test burns the same number on an idle machine and
/// on a loaded one. Wall-clock seconds are not, and deriving one from the other
/// made a busy machine fail tests that were doing nothing wrong.
///
/// A wall-clock kill still exists, but only as a backstop for an emulator that
/// has wedged rather than one that is merely slow, so it is set far above any
/// budget a test should need.
const WALL_LIMIT: Duration = Duration::from_secs(900);

/// What a program left behind when it stopped.
pub struct RunResult {
    /// The 16-bit value at `_exitcode`, formatted like the register scrape this
    /// replaced (four uppercase hex digits).
    pub value: String,
    /// Cycles between the start and end triggers, which z88dk-ticks prints on
    /// its last line.
    pub cycles: u64,
}

/// Run a program to its halt address and read its result out of a RAM dump.
///
/// `-trace` is the only way z88dk-ticks reports registers, and it prints every
/// executed instruction: emulation slows by more than two orders of magnitude,
/// enough to turn tests that finish in a third of a second into timeouts. The
/// harness crt0 instead stores main's return value at `_exitcode`, so the run
/// needs no trace at all and the value comes from `-output`, which dumps the
/// 64 KB address space at exit.
pub fn run_program(
    bin: &Path,
    target: Target,
    halt_addr: &str,
    result_addr: u32,
    dump: &Path,
    // How many emulated cycles this program may spend.
    cycles: u64,
) -> Result<RunResult, String> {
    // Emulation is the other expensive step; see `harness::pool`.
    let _permit = crate::harness::pool::acquire();

    let mut cmd = Command::new("z88dk-ticks");
    for flag in target.emu_flags() {
        cmd.arg(flag);
    }
    cmd.args(["-end", halt_addr]);
    cmd.args(["-counter", &cycles.to_string()]);
    cmd.arg("-output").arg(dump);
    cmd.arg(bin);
    cmd.stdout(Stdio::piped());
    cmd.stderr(Stdio::null());

    let mut child = cmd.spawn().map_err(|e| format!("z88dk-ticks: {e}"))?;
    let mut stdout = child.stdout.take().unwrap();
    let reader = std::thread::spawn(move || {
        let mut buf = String::new();
        let _ = std::io::Read::read_to_string(&mut stdout, &mut buf);
        buf
    });

    let start = std::time::Instant::now();
    let timeout = WALL_LIMIT;
    loop {
        match child.try_wait() {
            Ok(Some(_)) => break,
            Ok(None) => {
                if start.elapsed() > timeout {
                    let _ = child.kill();
                    let _ = child.wait();
                    return Err(format!("emulator wedged: no exit after {}s", WALL_LIMIT.as_secs()));
                }
                std::thread::sleep(Duration::from_millis(2));
            }
            Err(e) => return Err(format!("wait: {e}")),
        }
    }
    let spent = reader
        .join()
        .ok()
        .and_then(|out| out.lines().last().and_then(|l| l.trim().parse::<u64>().ok()))
        .unwrap_or(0);
    if spent >= cycles {
        return Err(format!("exhausted its {cycles} cycle budget without reaching _halt"));
    }

    let data = std::fs::read(dump).map_err(|e| format!("RAM dump: {e}"))?;
    let at = result_addr as usize;
    if at + 1 >= data.len() {
        return Err(format!("_exitcode at 0x{result_addr:04X} is outside the dump"));
    }
    // Little endian.
    let value = format!("{:04X}", u16::from(data[at]) | u16::from(data[at + 1]) << 8);
    Ok(RunResult { value, cycles: spent })
}




/// Run a program and report only that it reached its halt address.
///
/// The shipped crt0 does not record a result anywhere, so a program linked with
/// it can only be checked for booting: reaching `_halt` means the stack was set
/// up, the .bss loop terminated, and main was called and returned.
pub fn run_to_halt(
    bin: &Path,
    target: Target,
    halt_addr: &str,
    // How many emulated cycles this program may spend.
    cycles: u64,
) -> Result<(), String> {
    // Emulation is the other expensive step; see `harness::pool`.
    let _permit = crate::harness::pool::acquire();

    let mut cmd = Command::new("z88dk-ticks");
    for flag in target.emu_flags() {
        cmd.arg(flag);
    }
    cmd.args(["-end", halt_addr]);
    cmd.args(["-counter", &cycles.to_string()]);
    cmd.arg(bin);
    cmd.stdout(Stdio::piped());
    cmd.stderr(Stdio::null());

    let mut child = cmd.spawn().map_err(|e| format!("z88dk-ticks: {e}"))?;
    let mut stdout = child.stdout.take().unwrap();
    let reader = std::thread::spawn(move || {
        let mut buf = String::new();
        let _ = std::io::Read::read_to_string(&mut stdout, &mut buf);
        buf
    });
    let start = std::time::Instant::now();
    let timeout = WALL_LIMIT;
    loop {
        match child.try_wait() {
            Ok(Some(_)) => {
                // Exiting on the cycle counter is not the same as stopping at
                // the halt address; the emulator reports success either way.
                let spent = reader
                    .join()
                    .ok()
                    .and_then(|o| o.lines().last().and_then(|l| l.trim().parse::<u64>().ok()))
                    .unwrap_or(0);
                return if spent >= cycles {
                    Err(format!("exhausted its {cycles} cycle budget without reaching _halt"))
                } else {
                    Ok(())
                };
            }
            Ok(None) => {
                if start.elapsed() > timeout {
                    let _ = child.kill();
                    let _ = child.wait();
                    return Err(format!("emulator wedged: no exit after {}s", WALL_LIMIT.as_secs()));
                }
                std::thread::sleep(Duration::from_millis(2));
            }
            Err(e) => return Err(format!("wait: {e}")),
        }
    }
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
