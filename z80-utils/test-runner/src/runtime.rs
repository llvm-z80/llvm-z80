//! ELF-target runtime staging.
//!
//! The `--target=z80` / `--target=sm83` clang driver does not bundle crt0
//! or a default linker script, and there is no compiled compiler-rt
//! archive for these targets.  This module assembles the source assets
//! shipped under `compiler-rt/lib/builtins/{z80,sm83}/` into per-target
//! object files in the build tree, so the test runner can link tests
//! through `ld.lld` with crt0 + builtins + linker script.
//!
//! Staging is incremental: each `.asm` is reassembled only when the
//! corresponding `.o` is missing or older than its source.

use std::path::{Path, PathBuf};
use std::process::Command;

use crate::config::{Paths, Target};

pub struct ElfRuntime {
    pub crt0_obj: PathBuf,
    pub builtin_objs: Vec<PathBuf>,
    pub linker_script: PathBuf,
}

/// Assemble crt0.asm + the per-target builtin asm files into the build
/// tree so the test runner can link tests against them.  Idempotent and
/// incremental.
pub fn ensure_elf(paths: &Paths, target: Target, clang: &Path) -> Result<ElfRuntime, String> {
    let src_dir = paths.elf_builtins_src(target);
    let stage_dir = paths.elf_runtime_stage(target);
    let builtins_dir = stage_dir.join("builtins");

    if !src_dir.is_dir() {
        return Err(format!("compiler-rt source dir not found: {}", src_dir.display()));
    }

    std::fs::create_dir_all(&builtins_dir)
        .map_err(|e| format!("create {}: {e}", builtins_dir.display()))?;

    // The harness supplies its own startup code: it stores main's return value
    // at `_exit_code` so the runner can read the result from a RAM dump instead
    // of tracing every instruction. Otherwise identical to the shipped crt0.
    let crt0_src = paths.harness_crt0(target);
    let crt0_obj = stage_dir.join("crt0.o");
    assemble_if_stale(clang, target, &crt0_src, &crt0_obj)?;

    let mut builtin_objs = Vec::new();
    let entries = std::fs::read_dir(&src_dir)
        .map_err(|e| format!("read_dir {}: {e}", src_dir.display()))?;
    for entry in entries {
        let entry = entry.map_err(|e| format!("read_dir entry: {e}"))?;
        let path = entry.path();
        if path.extension().and_then(|s| s.to_str()) != Some("asm") {
            continue;
        }
        let stem = path.file_stem().and_then(|s| s.to_str()).unwrap_or("");
        // crt0 is linked separately; SDCC and CP/M-CRT0 variants belong to
        // other paths.
        if stem == "crt0" || stem == "crt0_sdcc" || stem.ends_with("_sdcc") || stem.starts_with("cpm_") {
            continue;
        }
        let obj = builtins_dir.join(format!("{stem}.o"));
        assemble_if_stale(clang, target, &path, &obj)?;
        builtin_objs.push(obj);
    }
    builtin_objs.sort();

    let linker_script = paths.elf_linker_script(target);
    if !linker_script.is_file() {
        return Err(format!("linker script not found: {}", linker_script.display()));
    }

    Ok(ElfRuntime {
        crt0_obj,
        builtin_objs,
        linker_script,
    })
}

/// Assemble the harness's SDCC-toolchain crt0 into the build tree. The suites
/// that link through sdldz80 need their own `.rel`, since the shipped crt0 does
/// not record main's return value.
pub fn ensure_sdcc_crt0(paths: &Paths, target: Target) -> Result<PathBuf, String> {
    let src = paths.harness_crt0_sdcc(target);
    if !src.is_file() {
        return Err(format!("harness crt0 not found: {}", src.display()));
    }
    let stage = paths.elf_runtime_stage(target);
    std::fs::create_dir_all(&stage).map_err(|e| format!("create {}: {e}", stage.display()))?;
    let obj = stage.join("harness_crt0.rel");
    if needs_rebuild(&src, &obj) {
        let status = Command::new(target.assembler())
            .arg("-g")
            .arg("-o")
            .arg(&obj)
            .arg(&src)
            .status()
            .map_err(|e| format!("{}: {e}", target.assembler()))?;
        if !status.success() {
            return Err(format!("{} failed on {}", target.assembler(), src.display()));
        }
    }
    Ok(obj)
}

fn assemble_if_stale(clang: &Path, target: Target, src: &Path, obj: &Path) -> Result<(), String> {
    if !needs_rebuild(src, obj) {
        return Ok(());
    }
    let status = Command::new(clang)
        .arg(format!("--target={}", target.triple()))
        .arg("-c")
        .arg(src)
        .arg("-o")
        .arg(obj)
        .status()
        .map_err(|e| format!("clang assemble {}: {e}", src.display()))?;
    if !status.success() {
        return Err(format!("clang failed to assemble {}", src.display()));
    }
    Ok(())
}

fn needs_rebuild(src: &Path, obj: &Path) -> bool {
    let obj_mtime = match std::fs::metadata(obj).and_then(|m| m.modified()) {
        Ok(t) => t,
        Err(_) => return true,
    };
    let src_mtime = match std::fs::metadata(src).and_then(|m| m.modified()) {
        Ok(t) => t,
        Err(_) => return true,
    };
    src_mtime > obj_mtime
}
