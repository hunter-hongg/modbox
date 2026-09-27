CHANGELOG

All notable changes to this project are documented in this file.

## Unreleased

### Added
- hexdump: add a command that displays file contents in hexadecimal, octal, decimal and ASCII. Format options `-b`, `-C`, `-c`, `-d`, `-o`, `-x` and `-X` are additive and interleave: each data block is printed once per requested format, in command-line order, before the next block begins, and the closing offset is written once at the end using the last format's width. Naming a format twice makes it print every block. `-n LEN` bounds the total bytes read, `-s OFFSET` skips into the stream with absolute offsets, and `-v` disables the duplicate-block squeeze, which compares line bodies per format and prints a single `*` per run. All files are concatenated into one continuous stream. Output was verified byte for byte against the reference `hexdump` over 2052 format/`-n`/`-s` combinations and a further 297 duplicate-squeeze cases, across input sizes from 0 to 200 bytes. No new dependencies.
- Man pages: add modbox-hexdump.

- pidof: add a command that lists the process IDs of running programs, highest PID first. Accepts multiple program names; a name containing a slash is matched against the exact `/proc/[pid]/exe` target, a bare name against the executable basename with a `comm` fallback for unreadable links and kernel threads. Options follow procps-ng `pidof`: `-s/--single-shot`, `-q` (exit-code-only), `-x` (shells running the named script), `-o/--omit-pid` (repeatable, comma-separated lists, and `%PPID`), `-S/--separator`. Output ordering, the self-exclusion rule and the exit-code contract (0 a PID is listed, 1 empty final list — including when `--omit-pid` removed every match — and 1 usage errors with invalid omit values) were verified differentially against procps-ng 4.0.7. `-c`, `-w` and `-t` are deliberately not implemented (see the man page NOTES). No new dependencies.
- tree: add a recursive directory-listing command that draws the hierarchy with box-drawing connectors and closes with a directory/file count. Listing: `-a`, `-d`, `-l` (cycle-safe: a link reaching an already-expanded directory is annotated `[recursive, not followed]`), `-f`, `-x`, `-L LEVEL`, `-i`, `--noreport`. Filtering: `-I`/`-P` as `|`-joined shell globs matched against the whole name. Metadata: `-s`, `--du`, `-h`, `-D`, `--inodes`, `--selinux`, `-F`, `--charset=ascii`. Ordering: `--sort=name|version|size|mtime|ctime|none` with `-v`, `-t`, `-c`, `-U`, `-r`, `--dirsfirst`, `--filesfirst`. Output matches GNU tree 2.3.2 byte for byte — verified differentially over 148 fixture cases and 165 listings of real directories, including human-readable size rounding, the six-month date switch, unprintable-name escaping and the exit-code contract (0 success, 1 usage, 2 a path could not be listed). No new dependencies; it reuses `fs_classify` and the size formatter already used by `du`.
- xxd: add a hexdump command that converts files into hexadecimal, postscript plain or C include format, or does the reverse. Supports `-a` autoskip, `-b`/`-d`/`-E`/`-e` digit and offset styles, `-c`/`-g` layout control, `-u` upper hex, `-s`/`-o`/`-l` offsets and lengths, `-i` with `-C`/`-n`/`-t`/`-ps` C-array and plain-hex output, and a full `-r` reverse path including patch mode and `-s` offsets. No new dependencies; dump, `-ps` and the `-r` reverse path match the reference `xxd` byte for byte (see the man page NOTES for the deliberate `-i` and `-h` deviations).
- tcpdump: add a packet capture and analysis command with three paths: live capture over Linux `AF_PACKET` raw sockets (`-i`, `-c N`, Ctrl-C summary via SIGINT/SIGTERM, EPERM error suggesting root/CAP_NET_RAW), classic pcap file reading (`-r FILE`/`-`, both endians, Wireshark-compatible) and writing (`-w FILE` with `-r`/`-w` round-trip equivalence and same-file conflict detection), and a decode chain for Ethernet II → ARP / IPv4 / IPv6 → TCP / UDP / ICMP / ICMPv6 producing tcpdump-style one-line-per-packet output. Filtering uses a user-space expression subset (`host`/`net`/`port`/`proto`/`src`/`dst` + `and`/`or`/`not` + parentheses, no BPF compiler), and display is controlled by `-q`, `-v`, `-e`, `-x`, `-tt`, `-s`, with `-n`/`-nn` accepted as no-ops. No libpcap and no new dependencies. Exit codes: 0 success, 1 runtime errors, 2 usage/parse errors. Verified by a fully deterministic test suite (fixtures byte-built with `xxd -r -p`, CI-safe without network or root).
- cmp: add a byte-by-byte file comparison command that is binary-safe (unlike the line-oriented `diff`). Supports `-b/--print-bytes`, `-i/--ignore-initial` (including the `SKIP1:SKIP2` and positional forms), `-l/--verbose`, `-n/--bytes`, `-s/--quiet`/`--silent`, reads `-` from stdin, and matches GNU exit codes (0 same, 1 differ, 2 trouble) and messages. No new dependencies.
- gunzip, unxz: register the traditional decompression aliases as first-class commands. `gunzip` decompresses by default (equivalent to `gzip -d`), `unxz` decompresses by default (equivalent to `xz -d`), following the `bunzip2`/`bzcat` pattern already used for the bzip2 family. Both keep the existing `-c`, `-d`, `-k`, `-f`, `-q`, `-v`, `-1`..`-9`, `--fast`, `--best`, `--help`, `--version` flags. No new compression code — the aliases are thin wrappers around the existing `gzip_command_impl` and `xz_command_impl`, with the invocation name passed through so `--help`/`--version` print the correct name.
- Man pages: add modbox-gunzip and modbox-unxz.
- flock: add a file-lock command for shell scripts (`util-linux` compatibility). Acquires an `flock(2)` lock on a file or directory then runs a command; exclusive (default) or `-s` shared, `-n` non-blocking, `-w` timeout, `-E` conflict-exit-code, `-o` close-fd, `-c`/`-F`, `--verbose`, plus an fd form (`flock 9`, `flock -u 9`). Zero new dependencies.
- namei: add a path-component diagnostic command (`util-linux` compatibility). Resolves 1..N paths one component at a time, labelling each with its type letter plus `-m` modes / `-o` owners / `-l` long / `-v` vertical / `-x` mount-points / `-n` no-symlink-follow; prints a `<name> - <error>` line and exits 1 on missing/inaccessible components; usage errors exit 2. Zero new dependencies.
- chrt: add a command that shows or changes the real-time scheduling policy and priority of a process, or launches a command under a chosen policy. Supports SCHED_OTHER/BATCH/IDLE/FIFO/RR (with SCHED_DEADLINE and SCHED_EXT recognised where the kernel provides them), the `--pid` query and set forms, `--max`, `--reset-on-fork`, `--all-tasks` and `--verbose`. Uses only POSIX scheduling calls, adding no dependencies, and its query/launch paths are testable without privileges.
- renice: add a command that alters the scheduling priority of running processes, selecting targets by process ID (default), process group ID (`-g`), or user name/UID (`-u`). Priorities are absolute by default, or relative with `--relative` (and `-n` when `POSIXLY_CORRECT` is set), with the kernel's `[-20, 19]` clamping reported. No new dependencies.
- logger: add a command that writes messages to the system log through the POSIX syslog interface, supporting `--priority` (facility.severity), `--tag`, `--file` (with `-` for stdin), `--stderr` and `--id`. No new dependencies and no privileges required; messages are read from arguments or stdin.
- findmnt: add a filesystem-query command that searches the mount table by device or mount point. Reads /proc/self/mountinfo (falling back to /proc/mounts), renders the mount hierarchy as a tree, and supports column selection, type/source/target filters, and table, list, pairs, raw, canonical and JSON output. Requires no privileges and adds no dependencies.
- which, whereis: add command-location utilities that resolve executables on PATH and locate binaries, sources and man pages.
- sdiff: add a side-by-side merge command that shows two files in parallel columns with `|`, `<`, `>` and `(` gutters marking the differences. Supports the comparison options `-i`, `-E`, `-Z`, `-b`, `-W`, `-B`, `-I`, `--strip-trailing-cr` and `-a`, the layout options `-l`, `-s`, `-t` and `-w`, and interactive merging into a file with `-o` (including editor commands `v`, `e` and their variants). Without `-o` it matches `diff -y` byte-for-byte, and it uses the same exit codes (0 same, 1 differ, 2 trouble) and messages as GNU diffutils. No new dependencies.
- bzip2, bunzip2, bzcat: add bzip2 compression, completing the compression family alongside gzip, xz and zstd. Supports compressing and decompressing in place, `-c/--stdout`, `-k/--keep`, `-f/--force`, `-t/--test` integrity checking, block-size levels `-1`..`-9` with `--fast`/`--best`, `-q`/`-v`, and stdin/stdout pipelines. `bunzip2` and `bzcat` are the same binary selecting a different default action from the name used to invoke it. The output is a standard bzip2 stream, verified interoperable with the system `bzip2`/`bunzip2`, including concatenated streams. Links the system `libbz2` through the same pkg-config mechanism already used by the gzip, xz and zstd commands.
- Man pages: add modbox-bc, modbox-man, modbox-setenforce, modbox-zcat, modbox-cmp, modbox-sdiff, modbox-bzip2, modbox-bunzip2, modbox-bzcat, modbox-gunzip, modbox-unxz, modbox-tcpdump, modbox-xxd. All commands now have man pages.

### Fixed

- pr: fix segfault on stdin input (string-literal mkstemp), add missing -t/-N/--columns/-d option support.
- od: add -b/-c/-o/-d/-x shortcut flags, fix -t format parsing, fix output layout (one address per line, final offset), fix signed decimal, fix hex address width, add -An/-Ax/-Ad bundled forms.
- kill: fix -l to accept signal names/numbers, fix -0 (existence check), fix error exit codes (0 -> 1).
- nslookup: fix --help requiring domain, make -t optional (default A).
- sleep: fix error exit code (0 -> 1).
- sum: fix error exit code on missing file (0 -> 1).
- stty: return exit 1 instead of 0 on a non-tty or unreadable device, name the requested device in the error when `-F` is given instead of falling back to "standard input", and handle the bare `speed`/`ispeed`/`ospeed` query forms.

### Changed
- Build: replace the external argtable3 dependency with an in-tree compatibility layer (`include/argtable3.h`, `src/argtable3.cpp`). It implements the slice of the API the commands use — `arg_lit0/1/n`, `arg_str0/1/n`, `arg_int0/n`, `arg_file0/1/n`, `arg_end`, `arg_parse`, `arg_parse_n`, `arg_print_errors`, `arg_freetable`, `arg_free` and `arg_print_glossary` — including `'|'`-separated long-option alternatives, short options that consume the following argv entry as their value, type-aware fallthrough so a positional `arg_int` does not swallow a filename, and the distinct `ARG_ENOMISSOPT` / `ARG_EBADNUM` codes. All ~130 command files compile against it unmodified and the argtable3 link is gone; the one dependency the first commit in this change records is the dispatcher fix that it accompanied. No behaviour change in the commands themselves.

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
