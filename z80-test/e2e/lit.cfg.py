# -*- Python -*-

import os
import resource
import shutil
import subprocess

import lit.formats
from lit.llvm import llvm_config

cpu = config.z80_cpu
opt = config.z80_opt
variant = config.z80_variant
fmt = config.z80_format
config.z80_variant_parts = variant.split("+") if variant else []

# Without this limit, each compiler crash leaves a core dump for the system
# to process.
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

config.name = "z80-e2e " + "-".join(p for p in (cpu, opt, variant, fmt) if p)
config.test_format = lit.formats.ShTest()
config.suffixes = [".c", ".ll"]
config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = config.z80_e2e_obj_root

if "Z80" not in config.targets_to_build.split(";"):
    config.unsupported = True

llvm_config.use_default_substitutions()
llvm_config.add_tool_substitutions(["split-file"], [config.llvm_tools_dir])

config.available_features.update([cpu, opt, fmt] + config.z80_variant_parts)

tools = config.llvm_tools_dir
triples = {"z80": "z80-unknown-none-sdcc", "sm83": "sm83-nintendo-none-sdcc"}
triple = cpu if fmt == "elf" else triples[cpu]
llc_flags = ["-z80-asm-format=sdasz80"] if fmt == "sdcc" else []

config.substitutions.append(
    (
        "%cc",
        " ".join(
            [
                os.path.join(tools, "clang"),
                "--target=" + triple,
                "-" + opt,
                "-ffreestanding",
                "-fno-crash-diagnostics",
                config.z80_clang_flags,
            ]
        ),
    )
)
config.substitutions.append(
    (
        "%llc",
        " ".join(
            [os.path.join(tools, "llc"), "-mtriple=" + cpu, "-" + opt]
            + llc_flags
            + [config.z80_llc_flags]
        ),
    )
)
config.substitutions.append(("%run", os.path.join(tools, "z80-run") + " --cpu=" + cpu))

if not os.path.exists(os.path.join(tools, "z80-run")):
    config.unsupported = True

if fmt == "sdcc":
    asm, link = ("sdasz80", "sdldz80") if cpu == "z80" else ("sdasgb", "sdldgb")
    if not shutil.which(asm) or not shutil.which(link):
        config.unsupported = True
    sdcc = shutil.which("sdcc")
    if sdcc:
        dirs = subprocess.run(
            [sdcc, "-m" + cpu, "--print-search-dirs"], capture_output=True, text=True
        ).stdout.splitlines()
        if "libdir:" in dirs and dirs.index("libdir:") + 1 < len(dirs):
            lib = os.path.join(dirs[dirs.index("libdir:") + 1].strip(), cpu + ".lib")
            if os.path.exists(lib):
                config.available_features.add("sdcc-compiler")
                config.substitutions.append(
                    ("%{sdcc}", sdcc + " -m" + cpu + " --std-c11")
                )
                config.substitutions.append(("%{sdcc-lib}", lib))
