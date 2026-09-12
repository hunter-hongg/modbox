CHANGELOG

All notable changes to this project are documented in this file.

## Unreleased

### Added

- cmp: add a byte-by-byte file comparison command that is binary-safe (unlike the line-oriented `diff`). Supports `-b/--print-bytes`, `-i/--ignore-initial` (including the `SKIP1:SKIP2` and positional forms), `-l/--verbose`, `-n/--bytes`, `-s/--quiet`/`--silent`, reads `-` from stdin, and matches GNU exit codes (0 same, 1 differ, 2 trouble) and messages. No new dependencies.
- chrt: add a command that shows or changes the real-time scheduling policy and priority of a process, or launches a command under a chosen policy. Supports SCHED_OTHER/BATCH/IDLE/FIFO/RR (with SCHED_DEADLINE and SCHED_EXT recognised where the kernel provides them), the `--pid` query and set forms, `--max`, `--reset-on-fork`, `--all-tasks` and `--verbose`. Uses only POSIX scheduling calls, adding no dependencies, and its query/launch paths are testable without privileges.
- renice: add a command that alters the scheduling priority of running processes, selecting targets by process ID (default), process group ID (`-g`), or user name/UID (`-u`). Priorities are absolute by default, or relative with `--relative` (and `-n` when `POSIXLY_CORRECT` is set), with the kernel's `[-20, 19]` clamping reported. No new dependencies.
- logger: add a command that writes messages to the system log through the POSIX syslog interface, supporting `--priority` (facility.severity), `--tag`, `--file` (with `-` for stdin), `--stderr` and `--id`. No new dependencies and no privileges required; messages are read from arguments or stdin.
- findmnt: add a filesystem-query command that searches the mount table by device or mount point. Reads /proc/self/mountinfo (falling back to /proc/mounts), renders the mount hierarchy as a tree, and supports column selection, type/source/target filters, and table, list, pairs, raw, canonical and JSON output. Requires no privileges and adds no dependencies.
- which, whereis: add command-location utilities that resolve executables on PATH and locate binaries, sources and man pages.
- sdiff: add a side-by-side merge command that shows two files in parallel columns with `|`, `<`, `>` and `(` gutters marking the differences. Supports the comparison options `-i`, `-E`, `-Z`, `-b`, `-W`, `-B`, `-I`, `--strip-trailing-cr` and `-a`, the layout options `-l`, `-s`, `-t` and `-w`, and interactive merging into a file with `-o` (including editor commands `v`, `e` and their variants). Without `-o` it matches `diff -y` byte-for-byte, and it uses the same exit codes (0 same, 1 differ, 2 trouble) and messages as GNU diffutils. No new dependencies.
- bzip2, bunzip2, bzcat: add bzip2 compression, completing the compression family alongside gzip, xz and zstd. Supports compressing and decompressing in place, `-c/--stdout`, `-k/--keep`, `-f/--force`, `-t/--test` integrity checking, block-size levels `-1`..`-9` with `--fast`/`--best`, `-q`/`-v`, and stdin/stdout pipelines. `bunzip2` and `bzcat` are the same binary selecting a different default action from the name used to invoke it. The output is a standard bzip2 stream, verified interoperable with the system `bzip2`/`bunzip2`, including concatenated streams. Links the system `libbz2` through the same pkg-config mechanism already used by the gzip, xz and zstd commands.
- Man pages: add modbox-bc, modbox-man, modbox-setenforce, modbox-zcat, modbox-cmp, modbox-sdiff, modbox-bzip2, modbox-bunzip2, modbox-bzcat. All 194 commands now have man pages.

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
- Tests: make the nice adjustment assertions skip gracefully when the session's base niceness is not 0, since the kernel then clamps every adjustment; the suite is now green even in an already-niced session, and the assertions still run on a normal host.
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
