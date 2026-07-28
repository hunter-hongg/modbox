# CoreUtils Gap: users, pinky, uptime, runcon, stdbuf

## Problem Statement

Modbox currently implements 92 of the 100 GNU CoreUtils commands. 5 commands remain unimplemented: `users`, `pinky`, `uptime`, `runcon`, and `stdbuf`. Users who rely on modbox as a drop-in CoreUtils replacement cannot use these commands.

## Solution

Implement each of the 5 missing commands following the same conventions as the existing modbox command set — each command gets a header file (`include/commands/<cmd>.hpp`), an implementation file (`src/commands/<cmd>.cpp`), uses `argtable3` for argument parsing, and registers itself via `REGISTER_COMMAND`. The Makefile discovers sources automatically, so no build-system changes are needed.

## User Stories

1. As a modbox user, I want to run `users` to see a space-separated list of logged-in user names, so that I can quickly see who is on the system.
2. As a modbox user, I want to run `pinky` to see lightweight finger-style user information, so that I can check a user's full name, office, and idle time without installing finger.
3. As a modbox user, I want to run `uptime` to see system uptime, number of users, and load averages, so that I can quickly assess system health.
4. As a modbox user, I want to run `runcon` to execute a command with a specified SELinux security context, so that I can test programs under different security labels.
5. As a modbox user, I want to run `stdbuf` to adjust standard I/O buffering for a command, so that I can control when output is flushed (e.g., line-buffered for real-time log processing).
6. As a modbox user, I want all 5 commands to support `--help` and follow the same option conventions as their GNU CoreUtils counterparts, so that muscle memory and scripts work unchanged.

## Implementation Decisions

### users

- Reads `/var/run/utmp` (or `WTMP_FILE`) via `getutent()` / `setutent()` / `endutent()`.
- Extracts `ut_user` for entries with `USER_PROCESS` type.
- Collects unique user names, sorts them, prints space-separated.
- Single `-h`/`--help` option.
- Prior art: `who.cpp` uses `<utmp.h>` and reads utmp entries.

### pinky

- Reads `/etc/passwd` via `getpwnam()` / `getpwent()` for user info.
- Reads `/var/run/utmp` for login status, idle time, and tty info.
- Supports:
  - `-l` long format (default)
  - `-b` brief format (short listing)
  - `-f` omit the remote hostname line
  - `-i` omit the user's full name
  - `-p` omit the user's plan file
  - `-s` short format (like `-b`)
  - `-h`/`--help`
- Idle time computed from `/proc/uptime` and utmp entry time.
- Prior art: `who.cpp` demonstrates utmp reading; `uptime` (in this spec) reads `/proc/uptime`.

### uptime

- Reads `/proc/uptime` for uptime seconds.
- Reads `/proc/loadavg` for 1/5/15-minute load averages.
- Counts logged-in users via utmp (`getutent()`).
- Formats output matching GNU Coreutils: `" 12:34:56 up 1 day,  2:30,  3 users,  load average: 0.00, 0.01, 0.05"`.
- Supports `-p`/`--pretty` for pretty-print format, `-s`/`--since` for system boot time.
- `-h`/`--help` option.
- Prior art: `ps.cpp` `ps_tui_read_uptime()` reads `/proc/uptime` and `/proc/loadavg`; `who.cpp` reads utmp.

### runcon

- Runs a command with a specified SELinux security context using `setexeccon()` and `execvp()`.
- Supports:
  - `-u`/`--user=USER` set user component
  - `-r`/`--role=ROLE` set role component
  - `-t`/`--type=TYPE` set type component
  - `-l`/`--range=RANGE` set range component
  - `--compute` compute process transition context
  - `-h`/`--help`
- Falls back to `execvp` if SELinux is not available (prints warning).
- Prior art: `chcon.cpp` uses `libselinux` context manipulation functions; `env.cpp` uses `fork`/`execvp` pattern.

### stdbuf

- Sets `stdbuf`-style buffering for a command by using `LD_PRELOAD` with the `libstdbuf.so` library (or setting `_STDBUF_E`, `_STDBUF_I`, `_STDBUF_O` environment variables).
- Since modbox uses `fork`/`execvp`, it sets the environment variables and then execs.
- Supports:
  - `-i`/`--input=MODE` adjust stdin buffering
  - `-o`/`--output=MODE` adjust stdout buffering
  - `-e`/`--error=MODE` adjust stderr buffering
  - MODE is `L`, `0` (unbuffered), or `N` (line-buffered for stdout, fully buffered for stderr).
  - `-h`/`--help`
- Newline-separated output ends with `\n`.
- Use `setenv` to set `_STDBUF_I`/`_STDBUF_O`/`_STDBUF_E` before exec.
- Prior art: `env.cpp` uses `fork`/`execvp` and environment manipulation.

## Testing Decisions

- Good tests verify external behavior: correct output format, correct exit codes, correct handling of missing files/args.
- Test at the command level via `tests/run_tests.sh` using `assert_cmd` / `assert_cmd_pat` helpers.
- No new test seams — existing Bash test framework is sufficient.
- For `uptime`: verify output matches pattern `/load average:.*\d+\.\d{2}/`.
- For `users`: verify output is a space-separated list of usernames.
- For `pinky`: verify `pinky root` prints user information.
- For `runcon`/`stdbuf`: verify basic `--help` output and that invocation passes through to the command.
- Prior art: existing tests in `tests/run_tests.sh` test `who`, `env`, `id`, etc. at the command level.

## Out of Scope

- `pinky` will not support the `-f` project file / `.plan` / `.project` file display on first implementation (read from home directory). These can be added later.
- `stdbuf` will not ship `libstdbuf.so`; it sets environment variables only. Full buffering control via `LD_PRELOAD` is a separate concern.
- `runcon` will not support `--compute` on first implementation.
- No multi-arch or cross-compilation concerns.

## Further Notes

- All 5 commands depend on Linux-specific facilities (`/proc/*`, utmp, SELinux). They will not compile or function on non-Linux platforms without `#ifdef` guards. Modbox currently has no portability layer — this is consistent with the existing codebase.
- `runcon` requires `libselinux` (already linked by modbox for `chcon`).
- `stdbuf` depends on the `_STDBUF_E`/`_STDBUF_I`/`_STDBUF_O` environment variables, which are honored by `libstdbuf.so` from GNU Coreutils. When `libstdbuf.so` is not present, the environment variables alone have no effect — this matches GNU Coreutils behavior.