# -*- Python -*-

import os

import lit.formats
from lit.llvm import llvm_config
from lit.llvm.subst import ToolSubst

config.name = "z80-tools"
config.test_format = lit.formats.ShTest()
config.suffixes = [".s", ".test"]
config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = config.z80_tools_obj_root

if "Z80" not in config.targets_to_build.split(";"):
    config.unsupported = True
for tool in ("z80-run", "ld.lld"):
    if not os.path.exists(os.path.join(config.llvm_tools_dir, tool)):
        config.unsupported = True

llvm_config.use_default_substitutions()
llvm_config.add_tool_substitutions(
    [
        "llvm-mc",
        "split-file",
        ToolSubst("ld.lld", unresolved="ignore"),
        ToolSubst("z80-run", unresolved="ignore"),
    ],
    [config.llvm_tools_dir],
)
