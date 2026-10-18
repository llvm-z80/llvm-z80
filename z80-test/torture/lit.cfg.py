# -*- Python -*-
#
# Parameters:
#   run-skipped  run only the skipped tests and fail the passing tests
#   verify       compile with -verify-machineinstrs

import os
import resource
import subprocess
import sys

sys.path.insert(0, os.path.dirname(__file__))
import torture

cpu = config.z80_cpu
opt = config.z80_opt
here = os.path.dirname(__file__)
root = config.z80_torture_source_dir
tools = config.llvm_tools_dir

# Without this limit, each compiler crash leaves a core dump for the system
# to process.
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

config.name = "z80-torture " + "-".join(p for p in (cpu, opt, config.z80_variant) if p)
config.test_format = torture.TortureTest()
config.test_source_root = root
config.test_exec_root = config.z80_torture_obj_root
# execute/builtins/lib holds the library halves of the builtins tests.
config.excludes = ["compat", "unsorted", "lib"]

config.torture_clang = os.path.join(tools, "clang")
config.torture_z80_run = os.path.join(tools, "z80-run")
# Declarations of the hosted headers the tests include, so that a test needing
# printf fails to link with an undefined symbol.
config.torture_include = os.path.join(here, "include")
config.torture_manifest = torture.Manifest(os.path.join(here, "manifest.txt"))
config.torture_run_skipped = lit_config.params.get("run-skipped") is not None
config.torture_extra_flags = config.z80_clang_flags.split()
if lit_config.params.get("verify") is not None:
    config.torture_extra_flags += ["-mllvm", "-verify-machineinstrs"]

if "Z80" not in config.targets_to_build.split(";"):
    config.unsupported = True
elif not os.path.isdir(os.path.join(root, "execute")):
    lit_config.note("%s: the gcc-torture submodule is not checked out" % config.name)
    config.unsupported = True
elif not os.path.exists(config.torture_clang) or not os.path.exists(
    config.torture_z80_run
):
    config.unsupported = True
else:
    os.makedirs(config.test_exec_root, exist_ok=True)
    config.torture_crt0 = os.path.join(config.test_exec_root, "crt0.o")
    subprocess.run(
        [
            config.torture_clang,
            "--target=" + cpu,
            "-c",
            os.path.join(here, "crt0", cpu + ".asm"),
            "-o",
            config.torture_crt0,
        ],
        check=True,
    )
    config.torture_flags = torture.accepted_flags(
        config.torture_clang, cpu, root, os.path.join(config.test_exec_root, "probe")
    )
