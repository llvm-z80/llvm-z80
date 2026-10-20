# lit test format for the GCC C torture tests.

import json
import os
import re
import shutil
import subprocess
from concurrent.futures import ThreadPoolExecutor

import lit.formats
import lit.Test
import lit.TestRunner

ICE = lit.Test.ResultCode("ICE", "Crashed", True)
COMPILE = lit.Test.ResultCode("COMPILE", "Rejected", True)
LINK = lit.Test.ResultCode("LINK", "Link Failed", True)
OPTIM = lit.Test.ResultCode("OPTIM", "Not Optimized", True)
TOOBIG = lit.Test.ResultCode("TOOBIG", "Too Large", True)
# Clang bugs are for upstream to fix, so a Clang bug does not fail the run.
CLANG = lit.Test.ResultCode("CLANG", "Clang Bug", False)
STALE = lit.Test.ResultCode("STALE", "Needlessly Skipped", True)

COMPILE_TIMEOUT = 60
LINK_TIMEOUT = 20
RUN_TIMEOUT = 300
# Enough for the slowest test that finishes, arith-rand-ll at -O0.
MAX_CYCLES = "40G"
# The stack size that `dg-add-options stack_size` promises a test. Some tests
# size their arrays from STACK_SIZE.
STACK_SIZE = 2048

REASONS = (
    "libc",
    "gcc",
    "unsupported",
    "softfloat-f64",
    "callbr",
    "int16",
    "size",
    "nonportable",
)
OPT_LEVELS = ("O0", "O1", "O2", "O3", "Os", "Oz")

MISSING_TARGETS = {
    "int32plus": "int16",
    "int32": "int16",
    "ptr32plus": "int16",
    "size32plus": "int16",
    "size20plus": "int16",
    "double64plus": "softfloat-f64",
    "trampolines": "gcc",
    "return_address": "gcc",
    "nonlocal_goto": "gcc",
    "fopenmp": "gcc",
    "fgraphite": "gcc",
    "fileio": "libc",
    "c99_runtime": "libc",
    "pthread": "libc",
    "mmap": "libc",
    "run_expensive_tests": "too-slow",
    # Some callees pop their own stack arguments, so a call with the wrong
    # arguments breaks the stack.
    "untyped_assembly": "unsupported",
    "dfp": "nonportable",
    "dfprt": "nonportable",
    "arm_arch_v5t_thumb_ok": "nonportable",
}
PRESENT_TARGETS = {"freestanding"}

# Under the machine verifier this test takes minutes and gigabytes to compile,
# so the verifier configurations skip the test.
VERIFY_SKIPS = {"compile/20001226-1.c"}

# The suite predates C99. GCC runs the suite with -fpermissive, and these
# flags demote the same errors to warnings. -w then hides the warnings.
PERMISSIVE_FLAGS = [
    "-Wno-error=implicit-function-declaration",
    "-Wno-error=implicit-int",
    "-Wno-error=int-conversion",
    "-Wno-error=incompatible-pointer-types",
    "-Wno-error=return-mismatch",
    "-Wno-error=declaration-missing-parameter-type",
    # Clang rejects `a > b > c`. GCC only warns.
    "-Wno-error=parentheses",
]


class Manifest:
    def __init__(self, path):
        self.rules = []
        with open(path) as f:
            for number, raw in enumerate(f, 1):
                line = raw.split("#", 1)[0].strip()
                if line:
                    self.rules.append(self._parse("%s:%d: " % (path, number), line))

    @staticmethod
    def _parse(where, line):
        lhs, colon, rhs = line.rpartition(":")
        if not colon:
            raise ValueError(where + "missing ':'")
        action, equals, text = rhs.strip().partition("=")
        text = text.strip()
        if not equals or action not in ("skip", "clang"):
            raise ValueError(where + "expected skip=<reason> or clang=<note>")
        if action == "skip" and text not in REASONS:
            raise ValueError(
                where + "unknown reason '%s' (known: %s)" % (text, ", ".join(REASONS))
            )
        words = lhs.split()
        if not words:
            raise ValueError(where + "missing pattern")
        cpu = opt = None
        for word in words[1:]:
            if word in OPT_LEVELS:
                opt = word
            elif word.lower() in ("z80", "sm83"):
                cpu = word.lower()
            else:
                raise ValueError(where + "unknown qualifier '%s'" % word)
        # A * matches any run of characters, slashes included.
        pattern = re.compile(".*".join(re.escape(p) for p in words[0].split("*")))
        return pattern, cpu, opt, action, text

    def lookup(self, action, name, cpu, opt):
        for pattern, rule_cpu, rule_opt, rule_action, text in self.rules:
            if (
                rule_action == action
                and rule_cpu in (None, cpu)
                and rule_opt in (None, opt)
                and pattern.fullmatch(name)
            ):
                return text
        return None


class Directives:
    def __init__(self, source):
        self.flags = []
        self.missing = None
        self.expects_error = False
        for line in source.splitlines():
            if "dg-" not in line:
                continue
            if "dg-error" in line:
                self.expects_error = True
            if "dg-require-effective-target" in line:
                rest = line.split("dg-require-effective-target", 1)[1]
                feature = re.match(r"\s*(\w*)", rest).group(1)
                if self.missing is None and feature in MISSING_TARGETS:
                    self.missing = (feature, MISSING_TARGETS[feature])
                continue
            for key in ("dg-additional-options", "dg-options"):
                if key in line:
                    quoted = line.split(key, 1)[1].split('"')
                    if len(quoted) > 1:
                        self.flags += quoted[1].split()
                    break


def _selector_matches(selector):
    negate = False
    for word in re.split(r"[\s{}]+", selector):
        word = word.strip('"')
        if not word:
            continue
        if word == "!":
            negate = True
            continue
        if word == "*-*-*":
            hit = not negate
        elif "-" in word:
            # Another architecture's triple.
            hit = False
        elif negate:
            hit = word in MISSING_TARGETS
        else:
            hit = word in PRESENT_TARGETS
        if hit:
            return True
        negate = False
    return False


def _skip_if_applies(rest, opt_flag):
    groups, depth, current = [], 0, []
    for c in rest:
        if c == "{":
            depth += 1
            if depth == 1:
                current = []
                continue
        elif c == "}":
            if depth == 1:
                groups.append("".join(current))
                depth = 0
                continue
            depth = max(depth - 1, 0)
        if depth == 1:
            current.append(c)
    if not groups or not _selector_matches(groups[0]):
        return False
    if len(groups) > 1:
        options = [w.strip('"') for w in groups[1].split()]
    else:
        tail = rest.split("}", 1)[1] if "}" in rest else ""
        options = [w.strip('"') for w in tail.split() if w.startswith('"')]
    options = [o for o in options if o]
    return not options or opt_flag in options


def dg_skip_if(source, opt):
    for line in source.splitlines():
        if "dg-skip-if" not in line:
            continue
        rest = line.split("dg-skip-if")[1]
        if not _skip_if_applies(rest, "-" + opt):
            continue
        quoted = rest.split('"')
        why = quoted[1].strip() if len(quoted) > 1 else ""
        return "unsupported (dg-skip-if: %s)" % (why or "dg-skip-if")
    return None


def dot_x_flags(path):
    try:
        with open(os.path.splitext(path)[0] + ".x") as f:
            lines = f.read().splitlines()
    except OSError:
        return []
    flags = []
    for line in lines:
        for prefix in ("set additional_flags", "lappend additional_flags"):
            if line.startswith(prefix):
                flags += [w.strip('"') for w in line[len(prefix) :].split()]
                break
    return [f for f in flags if f]


def flag_is_rejected(flag):
    # -m flags are for other targets, -O flags would override the level under
    # test, and the rest need tools or runtimes this target does not have.
    return (
        not flag.startswith("-")
        or flag.startswith("-m")
        or (flag.startswith("-O") and len(flag) <= 3)
        or flag.startswith("--param")
        or flag.startswith("-flto")
        or flag in ("-fopenmp", "-fsave-optimization-record")
    )


def test_flags(path, source):
    return [
        f
        for f in Directives(source).flags + dot_x_flags(path)
        if not flag_is_rejected(f)
    ]


def all_test_flags(root):
    flags = set()
    for tier in ("compile", "execute"):
        for dirpath, dirnames, filenames in os.walk(os.path.join(root, tier)):
            dirnames[:] = [d for d in dirnames if d != "lib"]
            for name in filenames:
                if name.endswith(".c"):
                    path = os.path.join(dirpath, name)
                    with open(path, errors="replace") as f:
                        flags.update(test_flags(path, f.read()))
    return flags


_accepted_flags = {}


def accepted_flags(clang, cpu, root, scratch):
    # The GCC-only flags that Clang accepts change between releases, so each
    # flag is tried once per CPU on a small file.
    key = (clang, cpu, root)
    if key not in _accepted_flags:
        _accepted_flags[key] = probe_flags(clang, cpu, all_test_flags(root), scratch)
    return _accepted_flags[key]


def probe_flags(clang, cpu, flags, scratch):
    os.makedirs(scratch, exist_ok=True)
    source = os.path.join(scratch, "probe.c")
    with open(source, "w") as f:
        f.write("int probe;\n")

    def accepted(item):
        index, flag = item
        obj = os.path.join(scratch, "probe%d.o" % index)
        cmd = [clang, "--target=" + cpu, "-c", flag, source, "-o", obj]
        try:
            return (
                subprocess.run(
                    cmd, capture_output=True, timeout=COMPILE_TIMEOUT
                ).returncode
                == 0
            )
        except subprocess.TimeoutExpired:
            return False

    flags = sorted(flags)
    with ThreadPoolExecutor(max_workers=8) as pool:
        verdicts = list(pool.map(accepted, enumerate(flags)))
    shutil.rmtree(scratch, ignore_errors=True)
    return {f for f, ok in zip(flags, verdicts) if ok}


def is_ice(stderr):
    markers = (
        "PLEASE submit a bug report",
        "LLVM ERROR",
        "UNREACHABLE executed",
        "Stack dump:",
        "ran out of registers",
        "Segmentation fault",
        "Bad machine code",
    )
    return any(m in stderr for m in markers) or (
        "Assertion" in stderr and "failed" in stderr
    )


def first_error(stderr):
    # The machine verifier prints the reasons before the error that ends the
    # compile, so the first reason is reported.
    lines = stderr.splitlines()
    for line in lines:
        if "Bad machine code:" in line:
            reason = line.split("Bad machine code:", 1)[1]
            return "bad machine code: " + reason.strip().rstrip("*").strip()
    for line in lines:
        if "error:" in line or "LLVM ERROR" in line:
            return line.strip()
    return next((l.strip() for l in lines if l.strip()), "error")


def undefined_symbols(stderr):
    symbols = []
    for line in stderr.splitlines():
        if "undefined symbol: " in line:
            symbol = line.split("undefined symbol: ", 1)[1].strip()
            if symbol and symbol not in symbols:
                symbols.append(symbol)
    return symbols


def is_marker(symbol):
    return re.fullmatch(r"_*link_error\d*", symbol) is not None


class TortureTest(lit.formats.TestFormat):
    def getTestsInDirectory(self, testSuite, path_in_suite, litConfig, localConfig):
        if not path_in_suite or path_in_suite[0] not in ("compile", "execute"):
            return
        source_dir = testSuite.getSourcePath(path_in_suite)
        for name in sorted(os.listdir(source_dir)):
            # A builtins test X.c is linked with X-lib.c. X-lib.c is not a
            # test.
            if name.endswith(".c") and not name.endswith("-lib.c"):
                yield lit.Test.Test(testSuite, path_in_suite + (name,), localConfig)

    def execute(self, test, litConfig):
        config = test.config
        # Only the built-in lit formats check config.unsupported.
        if config.unsupported:
            return lit.Test.Result(lit.Test.UNSUPPORTED, "Test is unsupported")
        name = "/".join(test.path_in_suite)
        tier = test.path_in_suite[0]
        cpu, opt = config.z80_cpu, config.z80_opt
        path = test.getSourcePath()
        with open(path, errors="replace") as f:
            source = f.read()
        dg = Directives(source)

        skip = config.torture_manifest.lookup("skip", name, cpu, opt)
        skip = skip or dg_skip_if(source, opt)
        # dg-require-effective-target applies to running a test, so the
        # compile tier ignores the directive.
        if not skip and tier == "execute" and dg.missing:
            skip = "%s (dg-require-effective-target %s)" % (
                dg.missing[1],
                dg.missing[0],
            )
        if not skip and name in VERIFY_SKIPS:
            if "-verify-machineinstrs" in config.torture_extra_flags:
                skip = "too slow under the machine verifier"
        if config.torture_run_skipped:
            if not skip:
                return lit.Test.Result(lit.Test.UNSUPPORTED, "not skipped")
        elif skip:
            return lit.Test.Result(lit.Test.UNSUPPORTED, skip)

        tmp = lit.TestRunner.getTempPaths(test)[1] + ".tmp"
        shutil.rmtree(tmp, ignore_errors=True)
        os.makedirs(tmp)
        flags = [f for f in test_flags(path, source) if f in config.torture_flags]
        code, detail = self._build_and_run(test, name, tier, tmp, flags)
        detail = detail.replace(path, name)

        if dg.expects_error:
            if code == COMPILE:
                code = lit.Test.XFAIL
            elif code == lit.Test.PASS:
                code, detail = lit.Test.XPASS, "no diagnostic, but dg-error expects one"
        if code.isFailure:
            note = config.torture_manifest.lookup("clang", name, cpu, opt)
            if note:
                code, detail = CLANG, "%s [%s]" % (note, code.name)
        if config.torture_run_skipped:
            if code == lit.Test.PASS:
                code, detail = STALE, "passes, but is skipped: " + skip
            else:
                code = lit.Test.UNSUPPORTED
        if not code.isFailure:
            shutil.rmtree(tmp, ignore_errors=True)
        return lit.Test.Result(code, detail)

    def _build_and_run(self, test, name, tier, tmp, flags):
        config = test.config
        sources = [test.getSourcePath()]
        if name.startswith("execute/builtins/") and tier == "execute":
            # builtins.exp links each test with the -lib.c half of the test
            # and with lib/main.c, the file with main.
            companion = sources[0][:-2] + "-lib.c"
            if os.path.isfile(companion):
                sources.append(companion)
            sources.append(os.path.join(os.path.dirname(sources[0]), "lib", "main.c"))

        objects = []
        for index, source in enumerate(sources):
            obj = os.path.join(tmp, "t%d.o" % index)
            failure = self._compile(test, source, obj, flags)
            if failure:
                return failure
            objects.append(obj)
        if tier == "compile":
            return lit.Test.PASS, ""

        elf = os.path.join(tmp, "t.elf")
        cmd = (
            [
                config.torture_clang,
                "--target=" + config.z80_cpu,
                "-nostartfiles",
                config.torture_crt0,
            ]
            + objects
            + ["-Wl,--gc-sections", "-o", elf]
        )
        # builtins.exp links the builtins tests this way, since the tests
        # define memcpy and other library functions.
        if name.startswith("execute/builtins/"):
            cmd.append("-Wl,--allow-multiple-definition")
        try:
            link = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                errors="replace",
                timeout=LINK_TIMEOUT,
                env=config.environment,
            )
        except subprocess.TimeoutExpired:
            return LINK, "link timed out after %d s" % LINK_TIMEOUT
        if link.returncode != 0:
            symbols = undefined_symbols(link.stderr)
            if symbols and all(is_marker(s) for s in symbols):
                return OPTIM, "not optimized away: " + " ".join(symbols)
            if "out of range" in link.stderr:
                return TOOBIG, "does not fit in the 64 KB address space"
            if symbols:
                return LINK, "undefined: " + " ".join(symbols)
            return LINK, first_error(link.stderr)

        report = os.path.join(tmp, "run.json")
        cmd = [
            config.torture_z80_run,
            "--report=" + report,
            "--max-cycles=" + MAX_CYCLES,
            elf,
        ]
        try:
            run = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                errors="replace",
                timeout=RUN_TIMEOUT,
                env=config.environment,
            )
        except subprocess.TimeoutExpired:
            return lit.Test.TIMEOUT, "z80-run did not finish in %d s" % RUN_TIMEOUT
        message = run.stderr.strip().replace("z80-run: %s: " % elf, "")
        if run.returncode == 0:
            return lit.Test.PASS, ""
        try:
            with open(report) as f:
                result = json.load(f)
        except (OSError, ValueError):
            result = {}
        if result.get("error") == "too-large":
            return TOOBIG, message
        if result.get("stop") in ("budget", "hang"):
            return lit.Test.TIMEOUT, message
        if run.returncode == 1:
            return lit.Test.FAIL, message
        return lit.Test.UNRESOLVED, message

    def _compile(self, test, source, obj, flags):
        config = test.config
        cmd = (
            [
                config.torture_clang,
                "--target=" + config.z80_cpu,
                "-" + config.z80_opt,
                "-c",
                "-w",
                "-fno-crash-diagnostics",
                "-std=gnu17",
                "-DSTACK_SIZE=%d" % STACK_SIZE,
            ]
            + PERMISSIVE_FLAGS
            + ["-I", config.torture_include]
            + config.torture_extra_flags
            # Last, so a test's own -std= wins.
            + flags
            + [source, "-o", obj]
        )
        try:
            cc = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                errors="replace",
                timeout=COMPILE_TIMEOUT,
                env=config.environment,
            )
        except subprocess.TimeoutExpired:
            return ICE, "compile timed out after %d s" % COMPILE_TIMEOUT
        if cc.returncode < 0:
            return ICE, "killed by signal %d" % -cc.returncode
        # The machine verifier can complain about a compile that succeeds.
        if is_ice(cc.stderr):
            return ICE, first_error(cc.stderr)
        if cc.returncode != 0 or not os.path.exists(obj):
            return COMPILE, first_error(cc.stderr)
        return None
