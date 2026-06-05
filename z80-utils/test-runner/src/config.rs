use std::fmt;
use std::path::{Path, PathBuf};
use std::process::Command;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Target {
    Z80,
    SM83,
}

impl Target {
    pub fn triple(&self) -> &'static str {
        match self {
            Target::Z80 => "z80",
            Target::SM83 => "sm83",
        }
    }

    pub fn sdcc_triple(&self) -> &'static str {
        match self {
            Target::Z80 => "z80-unknown-none-sdcc",
            Target::SM83 => "sm83-nintendo-none-sdcc",
        }
    }

    pub fn reg_name(&self) -> &'static str {
        match self {
            Target::Z80 => "DE",
            Target::SM83 => "BC",
        }
    }

    pub fn reg_grep(&self) -> &'static str {
        match self {
            Target::Z80 => "de",
            Target::SM83 => "bc",
        }
    }

    pub fn emu_flags(&self) -> &'static [&'static str] {
        match self {
            Target::Z80 => &[],
            Target::SM83 => &["-mgbz80"],
        }
    }

    pub fn emu_timeout_secs(&self) -> u64 {
        match self {
            Target::Z80 => 30,
            Target::SM83 => 30,
        }
    }

    pub fn assembler(&self) -> &'static str {
        match self {
            Target::Z80 => "sdasz80",
            Target::SM83 => "sdasgb",
        }
    }

    pub fn linker(&self) -> &'static str {
        match self {
            Target::Z80 => "sdldz80",
            Target::SM83 => "sdldgb",
        }
    }

    pub fn sdcc_flag(&self) -> &'static str {
        match self {
            Target::Z80 => "-mz80",
            Target::SM83 => "-msm83",
        }
    }
}

impl fmt::Display for Target {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.triple())
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum OptLevel {
    O0,
    O1,
    O2,
    O3,
    Os,
    Oz,
}

impl OptLevel {
    pub fn flag(&self) -> &'static str {
        match self {
            OptLevel::O0 => "-O0",
            OptLevel::O1 => "-O1",
            OptLevel::O2 => "-O2",
            OptLevel::O3 => "-O3",
            OptLevel::Os => "-Os",
            OptLevel::Oz => "-Oz",
        }
    }

    pub fn clang_flag(&self) -> &'static str {
        match self {
            OptLevel::O0 => "O0",
            OptLevel::O1 => "O1",
            OptLevel::O2 => "O2",
            OptLevel::O3 => "O3",
            OptLevel::Os => "Os",
            OptLevel::Oz => "Oz",
        }
    }

    pub fn parse(s: &str) -> Option<OptLevel> {
        match s {
            "O0" => Some(OptLevel::O0),
            "O1" => Some(OptLevel::O1),
            "O2" => Some(OptLevel::O2),
            "O3" => Some(OptLevel::O3),
            "Os" => Some(OptLevel::Os),
            "Oz" => Some(OptLevel::Oz),
            _ => None,
        }
    }
}

impl fmt::Display for OptLevel {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.clang_flag())
    }
}

#[derive(Clone)]
pub struct Paths {
    pub project_dir: PathBuf,
    pub build_dir: PathBuf,
}

impl Paths {
    pub fn resolve() -> Paths {
        // Project dir: directory containing Cargo.toml (or CWD)
        let project_dir = find_project_dir().unwrap_or_else(|| {
            std::env::current_dir().expect("cannot determine current directory")
        });

        let build_dir = std::env::var("BUILD_DIR")
            .map(PathBuf::from)
            .unwrap_or_else(|_| project_dir.join("../../build"));

        let build_dir = build_dir.canonicalize().unwrap_or(build_dir);

        Paths {
            project_dir,
            build_dir,
        }
    }

    pub fn clang(&self) -> PathBuf {
        self.build_dir.join("bin/clang")
    }

    pub fn llc(&self) -> PathBuf {
        self.build_dir.join("bin/llc")
    }

    pub fn clang_test_dir(&self) -> PathBuf {
        self.project_dir.join("testcases/clang")
    }

    pub fn sdcc_test_dir(&self) -> PathBuf {
        self.project_dir.join("testcases/sdcc")
    }

    pub fn custom_test_dir(&self) -> PathBuf {
        self.project_dir.join("testcases/custom")
    }

    pub fn llc_test_dir(&self) -> PathBuf {
        self.project_dir.join("testcases/llc")
    }

    pub fn crt0(&self, target: Target) -> PathBuf {
        let t = target.triple();
        self.build_dir.join(format!("lib/{t}/{t}_crt0.rel"))
    }

    pub fn rt_rel(&self, target: Target) -> PathBuf {
        let t = target.triple();
        self.build_dir.join(format!("lib/{t}/{t}_rt.rel"))
    }

    pub fn rt_lib(&self, target: Target) -> PathBuf {
        let t = target.triple();
        self.build_dir.join(format!("lib/{t}/{t}_rt.lib"))
    }
}

fn find_project_dir() -> Option<PathBuf> {
    // Try exe path first (works for installed binary)
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            // Workspace layout: exe is in <workspace>/target/debug/
            // test-runner dir is <workspace>/test-runner/
            if let Some(workspace) = dir.join("../..").canonicalize().ok() {
                let test_runner = workspace.join("test-runner");
                if test_runner.join("testcases").exists() {
                    return Some(test_runner);
                }
                // Old layout: testcases directly under project root
                if workspace.join("testcases").exists() {
                    return Some(workspace);
                }
            }
        }
    }
    // Fall back to CWD
    let cwd = std::env::current_dir().ok()?;
    // Workspace root: check test-runner subdir
    if cwd.join("test-runner/testcases").exists() {
        return Some(cwd.join("test-runner"));
    }
    if cwd.join("testcases").exists() {
        return Some(cwd);
    }
    None
}

/// Discover SDCC runtime library path.
pub fn find_sdcc_lib(target: Target) -> Option<PathBuf> {
    let output = Command::new("sdcc")
        .args([target.sdcc_flag(), "--print-search-dirs"])
        .output()
        .ok()?;
    let stdout = String::from_utf8_lossy(&output.stdout);
    let mut found_libdir = false;
    for line in stdout.lines() {
        if found_libdir {
            let dir = Path::new(line.trim());
            let lib = dir.join(format!("{}.lib", target.triple()));
            if lib.exists() {
                return Some(lib);
            }
            break;
        }
        if line.starts_with("libdir:") {
            found_libdir = true;
        }
    }
    None
}
