#!/usr/bin/env python3
"""Prints the lit results of the Z80 end-to-end and torture tests as a table
per suite, with a cell per configuration.

    LIT_OPTS="-o $PWD/build/results.json" ninja -C build check-z80-all
    z80-test/tools/matrix.py build/results.json
"""

import argparse
import collections
import json
import textwrap
import urllib.parse

# lit result codes that do not fail a run. Every other code fails the run.
PASSING = {"PASS", "FLAKYPASS", "FIXED", "XFAIL", "CLANG"}
NOT_RUN = {"UNSUPPORTED", "SKIPPED", "EXCLUDED"}
LABELS = {
    "FAIL": "Failed",
    "XPASS": "Unexpectedly Passed",
    "UNRESOLVED": "Unresolved",
    "TIMEOUT": "Timed Out",
    "ICE": "Crashed",
    "COMPILE": "Rejected",
    "LINK": "Link Failed",
    "OPTIM": "Not Optimized",
    "TOOBIG": "Too Large",
    "STALE": "Needlessly Skipped",
}
KINDS = ["z80-torture", "z80-e2e"]
CPUS = ["z80", "sm83"]
OPT_LEVELS = ["O0", "O1", "O2", "O3", "Os", "Oz"]
COMPONENTS = ["omit-fp", "no-static-frames", "inline-rt", "fast-math", "verify"]
COLORS = {"passed": "brightgreen", "failed": "red", "unsupported": "lightgrey"}
# These keep the output under the 1 MiB of a GitHub job summary.
MAX_GROUPS = 30
MAX_LISTED = 200

Config = collections.namedtuple("Config", "kind cpu opt variant format")
Table = collections.namedtuple("Table", "title columns rows")
Group = collections.namedtuple("Group", "kind tests where results configs")


def parse_suite(suite):
    kind, _, name = suite.partition(" ")
    if kind not in KINDS:
        return None
    parts = name.split("-", 2)
    variant = parts[2] if len(parts) > 2 else ""
    fmt = ""
    if kind == "z80-e2e":
        variant, _, fmt = variant.rpartition("-")
    components = frozenset(variant.split("+")) - {""}
    return Config(kind, parts[0], parts[1], components, fmt)


def rank(value, known):
    return (known.index(value) if value in known else len(known), value)


def order(values, known):
    return sorted(values, key=lambda v: rank(v, known))


def config_key(config):
    return (
        rank(config.cpu, CPUS),
        rank(config.opt, OPT_LEVELS),
        len(config.variant),
        order(config.variant, COMPONENTS),
        config.format,
    )


def variant_label(components):
    return "+".join(order(components, COMPONENTS)) or "base"


def config_label(config):
    names = [config.cpu, config.opt]
    if config.variant:
        names.append(variant_label(config.variant))
    if config.format:
        names.append(config.format)
    return "-".join(names)


def plural(count, noun):
    return "%d %s%s" % (count, noun, "" if count == 1 else "s")


def make_table(kind, fmt, configs, counts):
    common = frozenset.intersection(*(c.variant for c in configs))
    columns = {c.variant - common for c in configs}
    columns = sorted(columns, key=lambda v: (len(v), order(v, COMPONENTS)))
    title = "%s (%s)" % (kind, fmt) if fmt else kind
    rows = {(c.cpu, c.opt) for c in configs}
    rows = sorted(rows, key=lambda r: (rank(r[0], CPUS), rank(r[1], OPT_LEVELS)))
    cells = []
    for cpu, opt in rows:
        row = []
        for v in columns:
            stats = counts.get(Config(kind, cpu, opt, v | common, fmt))
            if stats is None:
                row.append(None)
            elif stats["failed"]:
                row.append(("failed", "%d failed" % stats["failed"]))
            elif stats["ran"]:
                row.append(("passed", "passed"))
            else:
                row.append(("unsupported", "unsupported"))
        cells.append(("%s %s" % (cpu, opt), row))
    return Table(title, [variant_label(v) for v in columns], cells)


def describe(chosen, universe):
    """Describes `chosen` by the fewest properties that select exactly
    `chosen` from `universe`, or returns None."""
    cpus = {c.cpu for c in chosen}
    opts = {c.opt for c in chosen}
    fmts = {c.format for c in chosen}
    present = frozenset.union(*(c.variant for c in chosen))
    properties = [
        ("cpu", ", ".join(order(cpus, CPUS)), lambda c: c.cpu in cpus),
        ("opt", ", ".join(order(opts, OPT_LEVELS)), lambda c: c.opt in opts),
        ("format", ", ".join(sorted(fmts)), lambda c: c.format in fmts),
    ]
    for part in order(frozenset.intersection(*(c.variant for c in chosen)), COMPONENTS):
        properties.append(("with", part, lambda c, part=part: part in c.variant))
    for part in order(
        frozenset.union(*(c.variant for c in universe)) - present, COMPONENTS
    ):
        properties.append(("without", part, lambda c, part=part: part not in c.variant))

    def picks(properties):
        return {c for c in universe if all(p[2](c) for p in properties)} == chosen

    if not picks(properties):
        return None
    for p in reversed(list(properties)):
        rest = [q for q in properties if q is not p]
        if picks(rest):
            properties = rest
    words = []
    for kind in ("cpu", "opt", "format", "with", "without"):
        values = [p[1] for p in properties if p[0] == kind]
        if values:
            prefix = kind + " " if kind in ("with", "without") else ""
            words.append(prefix + ", ".join(values))
    return "; ".join(words) or "every configuration"


def make_groups(failures, ran):
    """Groups the failing tests by the configurations of the failures. A
    group is described relative to the configurations that ran the tests."""
    by_configs = collections.defaultdict(list)
    for (kind, test), codes in failures.items():
        by_configs[kind, frozenset(codes)].append(test)
    merged = collections.defaultdict(list)
    for (kind, configs), tests in by_configs.items():
        universe = set().union(*(ran[kind, t] for t in tests))
        merged[kind, describe(configs, universe) or configs] += tests
    groups = []
    for (kind, where), tests in merged.items():
        configs = None
        if not isinstance(where, str):
            configs = [config_label(c) for c in sorted(where, key=config_key)]
            where = plural(len(configs), "configuration")
        results = collections.Counter(
            LABELS.get(code, code) for t in tests for code in failures[kind, t].values()
        )
        groups.append(Group(kind, sorted(tests), where, results, configs))
    groups.sort(key=lambda g: (-len(g.tests), g.tests[0]))
    return groups


def listed(items):
    shown = items[:MAX_LISTED]
    if len(items) > len(shown):
        shown.append("and %d more" % (len(items) - len(shown)))
    return shown


def badge(message, color, label=None):
    def escape(text):
        return urllib.parse.quote(text.replace("-", "--").replace("_", "__"))

    content = escape(message) + "-" + color
    alt = message
    if label:
        content = escape(label) + "-" + content
        alt = label + " " + message
    url = "https://img.shields.io/badge/%s?style=flat-square" % content
    return "![%s](%s)" % (alt, url)


def print_text(summary, tables, groups, rest):
    print(summary)
    for table in tables:
        print()
        print(table.title)
        lines = [[""] + table.columns]
        for label, row in table.rows:
            lines.append([label] + [cell[1] if cell else "" for cell in row])
        widths = [max(len(line[i]) for line in lines) for i in range(len(lines[0]))]
        for line in lines:
            print("  ".join(t.ljust(w) for t, w in zip(line, widths)).rstrip())
    if groups:
        print()
        print("Failures")
    for g in groups:
        results = ", ".join("%s %d" % r for r in g.results.most_common())
        print()
        print(
            "%s, %s, %s: %s" % (g.kind, plural(len(g.tests), "test"), g.where, results)
        )
        for names in (g.tests, g.configs or []):
            for line in textwrap.wrap(" ".join(listed(names)), 78):
                print("  " + line)
    if rest:
        print()
        print(rest)


def print_markdown(title, summary, tables, groups, rest):
    print("## " + title)
    print()
    print(summary)
    for table in tables:
        print()
        print("### " + table.title)
        print()
        print("| | " + " | ".join(table.columns) + " |")
        print("|:--" + "|:-:" * len(table.columns) + "|")
        for label, row in table.rows:
            cells = [badge(c[1], COLORS[c[0]]) if c else "" for c in row]
            print("| %s | %s |" % (label, " | ".join(cells)))
    if groups:
        print()
        print("### Failures")
    for g in groups:
        print()
        print("#### %s: %s, %s" % (g.kind, plural(len(g.tests), "test"), g.where))
        print()
        print(
            " ".join(
                badge(str(n), COLORS["failed"], r) for r, n in g.results.most_common()
            )
        )
        for noun, names in (("test", g.tests), ("configuration", g.configs)):
            if not names:
                continue
            text = ", ".join("`%s`" % n for n in listed(names))
            print()
            if len(names) <= 10:
                print(text)
                continue
            print("<details><summary>%s</summary>" % plural(len(names), noun))
            print()
            print(text)
            print()
            print("</details>")
    if rest:
        print()
        print(rest)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("results", help="the output of llvm-lit -o")
    parser.add_argument(
        "--markdown",
        action="store_true",
        help="print Markdown with badges for a GitHub job summary",
    )
    parser.add_argument(
        "--title", default="Z80 test results", help="the heading of the Markdown"
    )
    args = parser.parse_args()

    with open(args.results) as f:
        data = json.load(f)
    counts = collections.defaultdict(collections.Counter)
    failures = collections.defaultdict(dict)
    ran = collections.defaultdict(set)
    for result in data["tests"]:
        suite, _, test = result["name"].partition(" :: ")
        code = result["code"]
        config = parse_suite(suite)
        if config is None:
            continue
        counts[config]["tests"] += 1
        if code in NOT_RUN:
            continue
        counts[config]["ran"] += 1
        ran[config.kind, test].add(config)
        if code not in PASSING:
            counts[config]["failed"] += 1
            failures[config.kind, test][config] = code

    total = sum(s["tests"] for s in counts.values())
    failed = sum(s["failed"] for s in counts.values())
    elapsed = data.get("elapsed", 0)
    summary = "%d tests, %d failed, in %d min %d s." % (
        total,
        failed,
        elapsed // 60,
        elapsed % 60,
    )
    by_table = collections.defaultdict(list)
    for config in counts:
        by_table[config.kind, config.format].append(config)
    tables = [
        make_table(kind, fmt, by_table[kind, fmt], counts)
        for kind, fmt in sorted(by_table, key=lambda t: (KINDS.index(t[0]), t[1]))
    ]
    groups = make_groups(failures, ran)
    rest = ""
    if len(groups) > MAX_GROUPS:
        tests = sum(len(g.tests) for g in groups[MAX_GROUPS:])
        rest = "%s more with %s." % (
            plural(len(groups) - MAX_GROUPS, "group"),
            plural(tests, "test"),
        )
        groups = groups[:MAX_GROUPS]

    if args.markdown:
        print_markdown(args.title, summary, tables, groups, rest)
    else:
        print_text(summary, tables, groups, rest)


if __name__ == "__main__":
    main()
