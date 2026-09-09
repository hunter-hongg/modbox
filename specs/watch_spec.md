# Spec: Implement watch command

## Problem Statement

modbox ships 181 commands across coreutils, sysadmin, and networking, including a full procps process-monitoring cluster (`ps`, `top`, `mtop`, `htop`, `free`, `uptime`, `pgrep`, `pstree`, `vmstat`, `iostat`, `mpstat`). But users who want to repeatedly run a command and observe its output over time — the classic `watch df -h` / `watch ls -la` workflow — must fall back to an external `watch` binary. That defeats the purpose of a BusyBox-style single binary, especially in minimal containers and rescue images where modbox is often the only binary present.

## Solution

Implement `watch` as a modbox command that repeatedly executes a given COMMAND, captures its standard output, and redisplays it (clearing the screen when attached to a terminal). The command is run through `sh -c` by default (args joined with spaces, GNU-compatible) or directly via `execvp` with `-x`. Options cover the GNU/procps surface used most in practice: interval control with unit suffix, a title header, beep/errexit/chgexit termination behaviors, and change highlighting.

## User Stories

1. As a sysadmin, I want `modbox watch df -h` to run `df -h` every 2 seconds and redisplay the output, so I can monitor disk usage changing over time.
2. As a developer, I want `modbox watch -n 0.5 ls -la` to refresh every half second, so I can see directory changes quickly.
3. As a user, I want the title bar `Every 2.0s: df -h` plus the hostname and current date shown above the output, so I know what is being watched and when the frame was rendered.
4. As a pipeline author, I want the command arguments preserved verbatim after the first non-option token, so options belonging to the watched command are not swallowed by `watch`.
5. As a user, I want `modbox watch -t COMMAND` to hide the title header, so the watched output fills the window.
6. As a script writer, I want `modbox watch -e false` to stop as soon as the command returns non-zero (exiting with the command's status), so a failing probe fails the pipeline immediately.
7. As a script writer, I want `modbox watch -g 'date +%s%N'` to stop as soon as the command's output changes, so I can detect state transitions.
8. As a user, I want `modbox watch -b COMMAND` to beep when the command exits non-zero, so I notice failures without staring at the screen.
9. As a user, I want `modbox watch -x COMMAND ARG...` to exec the command directly instead of wrapping it in `sh -c`, so quoting and signal behavior match direct execution.
10. As a user, I want `modbox watch -n 1m COMMAND` to accept `s`/`m`/`h`/`d` interval suffixes, so I can express minutes and hours naturally.
11. As a user, I want a blank-line-free repeated output when stdout is not a terminal (no ANSI full-screen control sequences), so piping watch output into another tool is clean.
12. As a user, I want `modbox watch -d COMMAND` to highlight lines that changed since the previous run when attached to a terminal, so changes stand out.
13. As a user, I want `modbox watch --help` / `--version` to show usage and the modbox version, so the command is discoverable.
14. As a tester, I want `modbox watch` with no command, or with an unknown option, to exit 2 with a usage hint, so error handling matches the other modbox commands.
15. As a user, I want `watch` to appear in `modbox help`, so it shows up in the command listing.

## Implementation Decisions

### Architecture

- New header: `include/commands/watch.hpp` declaring `int watch_command(int argc, char** argv);` with include guard `WATCH_HPP`.
- New source: `src/commands/watch.cpp` implementing the command end-to-end. No new shared helper module — STDIN plumbing, `sh -c` joining, and the poll-and-print loop are all local.
- Registration: `REGISTER_COMMAND("watch", watch_command, "Execute a program periodically, showing output fullscreen")` at the bottom of the file.
- No new `PKGS` entries: only libc + existing stdlib. `ioctl(TIOCGWINSZ)` for terminal width, `ctime()` for the title date, `poll()` for concurrent pipe draining — all thin POSIX wrappers on the standard library.

### WATCHOptions struct

Follows the C++20 default-initializer convention from AGENTS.md; passed to helpers as `const WATCHOptions*` (pointer, not reference):

```cpp
struct WATCHOptions {
    bool no_title = false;     // -t
    bool beep = false;         // -b
    bool errexit = false;      // -e
    bool chgexit = false;      // -g
    bool differences = false;  // -d[=permanent]
    bool exec_mode = false;    // -x
    double interval = 2.0;     // -n
    bool is_tty = false;
};
```

### CLI parsing

- Hand-rolled option loop in the style of `stdbuf.cpp` / `timeout.cpp` (options first; the first non-option token starts the command; a bare `--` ends option parsing). This is required because the trailing `COMMAND [ARG]...` must be preserved verbatim and may itself contain option-looking tokens.
- Short options: `-b`, `-d`, `-e`, `-g`, `-t`, `-x` (lits); `-n <secs>` (value, glued form `-n0.5` accepted); `-h`/`--help`; `-v`/`--version`.
- `-d` accepts an optional argument: `-d`, `--differences`, `-dpermanent`, or `--differences=permanent`. Any other value after `-d` is an error. The space-separated form `-d permanent` deliberately follows GNU getopt optional-argument semantics: the optional value is only consumed when glued to the flag, so `-d permanent` treats `permanent` as the command (verified against GNU watch 4.x on this machine).
- Long options: `--beep`, `--differences[=permanent]`, `--errexit`, `--chgexit`, `--interval=<secs>`, `--no-title`, `--exec`, `--help`, `--version`.
- `--interval` value parsed with suffix support (`s`/`m`/`h`/`d`, seconds default), reusing the `parse_duration` semantics from `timeout.cpp`. Valid range: > 0 and < some sane bound (reject <= 0; reject absurd magnitudes with "invalid interval" + usage error, exit 2).
- Unrecognized option / missing command → `watch: invalid option -- 'x'` (or `watch: missing command`) on stderr + `Try 'watch --help' for more information.` + exit 2.

### Command execution

- Default mode: join `argv[cmd_start..]` with single spaces into `cmdline`, run `sh -c cmdline`.
- `-x` mode: `execvp(argv[cmd_start], &argv[cmd_start])` directly.
- Child process: stdout is `dup2`'d to a pipe; **stderr is inherited** (goes to watch's stderr), matching GNU watch. On exec failure (default mode: `sh` itself missing is practically impossible; `-x` mode: `execvp` failure) the child prints an error and `_exit(127)`.
- Parent: drains the pipe concurrently via `poll()` while `waitpid(WNOHANG)` checks for child exit; EOF + child reaped → frame complete. This avoids a deadlock for single frames larger than the pipe buffer and mirrors the `poll()` relay pattern already used by `nc.cpp`.
- Frame data: captured stdout text + child exit status. The command is **not** re-run until the next interval tick.

### Display / title

- Non-TTY (`!isatty(STDOUT_FILENO)`): no clear-screen sequences at all. Print the title line (unless `-t`) followed by the captured output. This deviates deliberately from GNU watch (which always emits ncurses/ANSI sequences) to keep piped output clean and the test suite deterministic; documented in the man page.
- TTY: before each frame print `\033[H\033[2J` (home + clear screen), then the title (unless `-t`), then the output. On exit, restore the cursor (`\033[?25h`) — the alternate-screen cursor-hide sequence is not used, so no restore-on-SIGINT plumbing is required.
- Title line: `Every <I>s: <command>` where `<I>` is the interval formatted with `%.1f` (GNU-style; `2.0` → `Every 2.0s:`), padded with spaces on the right so that `<hostname>, <date>` sits at the right edge. Width comes from `ioctl(TIOCGWINSZ)` when a TTY, else a fallback of 80; long commands are not truncated (padding stops at column 1).
- Date uses `ctime()` (e.g. `Thu Sep 10 06:08:58 2026`), trailing newline stripped; hostname via `gethostname()`.

### Exit behaviors

- `-e`/`--errexit`: when the command exits non-zero, stop and exit with the command's status.
- `-g`/`--chgexit`: when the captured output text differs from the previous iteration, stop and exit 0. The first iteration establishes the baseline (never exits on iteration one).
- `-b`/`--beep`: when the command exits non-zero, emit a BEL (`\007`) to stdout before the next frame.
- `-d`/`--differences[=permanent]`: when stdout is a TTY, lines whose text differs from the same line index in the previous frame (or frames whose line count changed) are wrapped with `\033[7m` ... `\033[0m` (reverse video). When not a TTY, the option is accepted but applies no styling. `permanent` is parsed and accepted; v1 applies the same per-frame highlight for both forms (a persistent marker is a later enhancement).
- Normal termination (no `-e`/`-g` trigger): the loop runs until interrupted by a signal (SIGINT/SIGTERM kill the process in the default way).
- Usage errors exit 2. `--help` exits 0.

### Interval and sleeping

- `sleep` between the end of frame N and the start of frame N+1 (`nanosleep`, restarting on `EINTR`). `-p`/`--precise` (accounting for command runtime) is out of scope for v1.

## Testing Decisions

- **Good test**: asserts external behavior only — exit code, stdout content/patterns, stderr patterns. Timing-dependent assertions (interval cadence) use generous `timeout` wrappers and pattern counts rather than exact output.
- **Modules under test**: `src/commands/watch.cpp` (black-box via `modbox watch ...`).
- **Seam**: the runner's `TMPDIR`-setting and CWD-at-repo-root conventions apply; all watch tests run non-TTY so captures are deterministic. Live timing tests are wrapped in `"$MODBOX" timeout <N> ...` so they cannot hang the parallel suite.
- **Prior art**: `tests/test_ping.sh` (conditional live tests), `tests/test_nc.sh` (background processes + bounds), `tests/test_stdbuf.sh` patterns (run command with options then a trailing command).
- **Tests to include** (in `tests/test_watch.sh`):
  1. `watch --help` shows a `Usage:` line listing options.
  2. `watch -h` shows usage.
  3. `watch --version` prints the modbox version.
  4. `watch -v` prints the modbox version.
  5. `watch` with no args → exit 2.
  6. `watch --bogus ...` → exit 2 with "unrecognized"/"invalid option".
  7. `watch -n abc echo hi` → exit 2.
  8. `watch -n 0 echo hi` → exit 2 (interval must be > 0).
  9. `timeout 1 watch -n 0.1 echo hello` → stdout contains the title `Every 0.1s: echo hello` and at least two `hello` lines.
  10. `timeout 1 watch -t -n 0.1 echo hi` → stdout contains `hi` and does **not** contain `Every`.
  11. `timeout 1 watch -n 0.1 'echo $((40+2))'` → output contains `42` (proves the `sh -c` join path runs the args as a shell command).
  12. `watch -x -n 0.1 sh -c 'exit 3'` wrapped in time bound → the shell command runs via exec (title shows the args joined); with `-e` it exits 3.
  13. `watch -e -n 0.1 false` → exits quickly with status 1.
  14. `watch -g -n 0.1 date +%s%N` → exits quickly with status 0.
  15. `timeout 1 watch -b -e -n 0.1 false` → stdout contains a BEL (`\007`) character.
  16. `timeout 1 watch -d -n 0.1 echo hi` → parsed and still runs (output contains `hi`); no styling markup in non-TTY capture.
  17. `timeout 1 watch -n 0.1s echo hi` → `0.1` suffix accepted (output matches `Every 0.1s: echo hi`).
  18. Non-TTY capture contains no escape (ESC `\x1b`) characters for the plain (non-`-d`) case.
  19. `watch` appears in `modbox help`.

## Out of Scope

- `-p`/`--precise` (interval anchored to wall clock).
- `-f`/`--follow`, `-q`/`--equexit`, `-r`/`--no-rerun`, `-s`/`--shotsdir`, `-C`/`--no-color` (newer procps options).
- Full ANSI cursor-bookkeeping for `-c`/`--color` (interpreted color output).
- Interactive key handling (`q` to quit, etc.) — v1 relies on the terminal's copy mode / Ctrl-C.
- `permanent` differential semantics beyond the shared per-frame highlight.
- Truncation/wrapping control (`-w`/`--no-wrap`).
- Piping watch's own stdin through to the watched command with interactive paging.

## Further Notes

- C++20 STL only, no GLib.
- `(void)fprintf(...)` cast style for return-value suppression.
- `const WATCHOptions*` (pointer, not reference) per AGENTS.md.
- Use `print_version("watch")` from `version_util.hpp` for `--version`.
- Reuse the `parse_duration`-style suffix parser from `timeout.cpp` (inlined, not exported).
- Register man page `docs/man/modbox-watch.1.md` in `Makefile` `MAN_SOURCES` (append after `modbox-netcat.1.md`).
- Add `watch` to `registered_cmds.txt` (alphabetically between `wall` and `wc`) and update its header count.
- Add `watch` to the README command list (between `wall` and `wc`) and bump the command count 181 → 182.
- Add AGENT_CHANGELOG.md entry.