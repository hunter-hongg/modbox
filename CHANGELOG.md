CHANGELOG

All notable changes to this project are documented in this file.

## Unreleased

### Added

- logger: add a command that writes messages to the system log through the POSIX syslog interface, supporting `--priority` (facility.severity), `--tag`, `--file` (with `-` for stdin), `--stderr` and `--id`. No new dependencies and no privileges required; messages are read from arguments or stdin.
- findmnt: add a filesystem-query command that searches the mount table by device or mount point. Reads /proc/self/mountinfo (falling back to /proc/mounts), renders the mount hierarchy as a tree, and supports column selection, type/source/target filters, and table, list, pairs, raw, canonical and JSON output. Requires no privileges and adds no dependencies.
- which, whereis: add command-location utilities that resolve executables on PATH and locate binaries, sources and man pages.
- Man pages: add modbox-bc, modbox-man, modbox-setenforce, modbox-zcat. All 186 commands now have man pages.

### Fixed

- pr: fix segfault on stdin input (string-literal mkstemp), add missing -t/-N/--columns/-d option support.
- od: add -b/-c/-o/-d/-x shortcut flags, fix -t format parsing, fix output layout (one address per line, final offset), fix signed decimal, fix hex address width, add -An/-Ax/-Ad bundled forms.
- kill: fix -l to accept signal names/numbers, fix -0 (existence check), fix error exit codes (0 -> 1).
- nslookup: fix --help requiring domain, make -t optional (default A).
- sleep: fix error exit code (0 -> 1).
- sum: fix error exit code on missing file (0 -> 1).

### Changed

- Spec: `tar` command (ustar + GNU longname + pax, streaming -z/-j/-J compression, `-f -` pipes). See docs/specs/tar-command.md.
- tar: fix mtime preservation (fflush before futimens), recursive directory walk, path traversal rejection, --xz/--zstd long option mapping, pax reader newline handling, pax writer length calculation, uid/gid field thresholds, parse_args nits, and housekeeping (man pages, registered_cmds, Makefile, tests).
- Tests: add tests for kill, od, pr, printenv, realpath, shred, sleep, sum, nslookup (72 new assertions).
- Tests: make perf stat exit-status assertions skip gracefully when the kernel forbids unprivileged perf events (perf_event_paranoid > 1), so the suite is green regardless of host policy.
- Docs: refresh registered_cmds.txt, man_pages.txt, specs/missing_commands_overview.md to reflect current state.

## v0.1.0 (2026-08-04)
-----------------------
- Added `fd` command: recursive file search with regex/glob, filters, color, and exec support. (Implemented in src/commands/fd.cpp)
- Added interactive `ls --tui` and `lf` alias: two-pane file browser using ftxui. (src/commands/ls_tui.cpp, src/commands/lf.cpp)
- Introduced shared TUI base class used by multiple TUIs (include/commands/tui_base.hpp, src/commands/tui_base.cpp).
- Enhanced `cp` with multiple options and correctness improvements: recursive copy, -f, -n, -i, -u, -p, -t, preserve semantics. (src/commands/cp.cpp)
- Added tests and TUI fallbacks; comprehensive test suite passes locally (see tests/run_tests.sh).
- Added GitHub Actions CI workflow: .github/workflows/ci.yml

Notes
-----
See docs/superpowers/STATUS-2026.md for a cross-referenced implementation status and suggested follow-ups (polish, docs reconciliation, changelog tasks).

For contributors
----------------
- Run the test suite locally: bash tests/run_tests.sh
- Build: make
- Add issues for follow-ups or pick items from docs/superpowers/TODOS-2026.md
