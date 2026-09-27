#!/usr/bin/env python3
#
# regen_registry.py — regenerate the derived command-inventory files.
#
# `registered_cmds.txt` and `man_pages.txt` are git-ignored generated files,
# but they are the *only* input to the registry-driven man-page coverage check
# in tests/test_man_pages.sh. Nothing in the build regenerated them, so adding
# a command without hand-editing both files silently shrank the coverage
# assertion to the stale set — the check kept passing while covering less.
#
# The binary is the single source of truth: the command set is whatever
# REGISTER_COMMAND registered, so it is read back out of `modbox help` rather
# than parsed out of the sources (which would need to model the macro's
# argument syntax and any conditional registration).
#
# Usage:
#   python3 tests/regen_registry.py            # write the files
#   python3 tests/regen_registry.py --check    # exit 1 if stale (CI use)
#
# Requires a built binary; override its location with MODBOX=path/to/modbox.

import argparse
import os
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO_ROOT = HERE.parent

# `modbox help` lists one command per line as " name    Description".
# The name alphabet is the one REGISTER_COMMAND accepts.
HELP_NAME_RE = re.compile(r"^ ([a-zA-Z0-9[]+) ")

# `[` is an alias of `test` and is documented by modbox-test(1), so it is
# excluded from the inventory to keep the man-page check one-command-per-page.
ALIASES_EXCLUDED_FROM_INVENTORY = {"["}


def registered_commands(binary):
    """The command set, read back out of the built binary."""
    out = subprocess.run(
        [str(binary), "help"],
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=True,
    ).stdout.decode("utf-8", "replace")

    names = set()
    for line in out.splitlines():
        m = HELP_NAME_RE.match(line)
        if m:
            names.add(m.group(1))
    if not names:
        raise SystemExit("no commands parsed from `%s help` — bad output?" % binary)
    return sorted(names - ALIASES_EXCLUDED_FROM_INVENTORY)


def build_registered_cmds(names):
    body = "\n".join(names)
    return (
        "# registered_cmds.txt - %d commands (regenerated from REGISTER_COMMAND in src;\n"
        "# `[` is an alias for `test` and is covered by its man page)\n"
        "%s\n" % (len(names), body)
    )


def build_man_pages(names, date):
    return ("# man_pages.txt - %d entries (%s, based on docs/man/*.md)\n%s\n"
            % (len(names), date, "\n".join(names)))


def missing_docs(names):
    """Commands with neither a man page source nor a Makefile MAN_SOURCES entry.

    This is the same contract tests/test_man_pages.sh asserts, checked here
    too so `make regen` reports a documentation gap instead of writing an
    inventory that the next test run will fail on.
    """
    makefile = (REPO_ROOT / "Makefile").read_text()
    gaps = []
    for cmd in names:
        if not (REPO_ROOT / "docs" / "man" / ("modbox-%s.1.md" % cmd)).exists():
            gaps.append("no man page:   %s" % cmd)
        if ("modbox-%s.1.md" % cmd) not in makefile:
            gaps.append("no MAN_SOURCES: %s" % cmd)
    return gaps


def main():
    ap = argparse.ArgumentParser(description="regenerate modbox command inventory files")
    ap.add_argument("--check", action="store_true",
                    help="do not write; exit 1 if the files are stale")
    args = ap.parse_args()

    binary = os.environ.get("MODBOX") or (REPO_ROOT / "target" / "modbox")
    if not pathlib.Path(binary).exists():
        raise SystemExit("binary not found: %s (run `make` first)" % binary)

    names = registered_commands(binary)
    outputs = {
        REPO_ROOT / "registered_cmds.txt": build_registered_cmds(names),
        REPO_ROOT / "man_pages.txt": build_man_pages(names, _today()),
    }

    for gap in missing_docs(names):
        print("WARNING: %s" % gap, file=sys.stderr)

    stale = [p for p, want in outputs.items()
             if not p.exists() or p.read_text() != want]
    if not stale:
        print("inventory up to date (%d commands)" % len(names))
        return 0

    if args.check:
        for p in stale:
            print("STALE: %s" % p.relative_to(REPO_ROOT), file=sys.stderr)
        return 1

    for p, want in outputs.items():
        p.write_text(want)
        print("wrote %s (%d commands)" % (p.relative_to(REPO_ROOT), len(names)))
    return 0


def _today():
    import datetime
    return datetime.date.today().isoformat()


if __name__ == "__main__":
    sys.exit(main())
