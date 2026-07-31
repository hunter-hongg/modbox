# ModBox: GNU CoreUtils Compatibility - Missing Commands Implementation Overview

## Problem Statement

ModBox implements 129 commands including all standard GNU CoreUtils commands plus additional utilities. Only 2 standard coreutils commands remain unimplemented: `pinky` and `stdbuf`. Users who rely on modbox as a drop-in CoreUtils replacement cannot use these commands.

## Already Implemented

The following commands were identified in the original missing commands overview and are now implemented:

- **hostname** — Get or set the system host name (`src/commands/hostname.cpp`)
- **pathchk** — Check file names for portability and validity (`src/commands/pathchk.cpp`)
- **readlink** — Read symbolic links (`src/commands/readlink.cpp`)
- **users** — Print user names of users currently logged in (`src/commands/users.cpp`)
- **uptime** — Print system uptime and load averages (`src/commands/uptime.cpp`)
- **runcon** — Run command with specified security context (`src/commands/runcon.cpp`)
- **printenv** — Print environment variables (`src/commands/printenv.cpp`)
- **sha224sum** — Compute SHA224 checksum (`src/commands/sha224sum.cpp`)
- **sha384sum** — Compute SHA384 checksum (`src/commands/sha384sum.cpp`)
- **sha512sum** — Compute SHA512 checksum (`src/commands/sha512sum.cpp`)
- **truncate** — Shrink or extend files to specified size (`src/commands/truncate.cpp`)
- **umask** — Set or read file creation permission mask (`src/commands/umask.cpp`)
- **wall** — Write a message to all logged-in users (`src/commands/wall.cpp`)
- **who** — Show who is logged in (`src/commands/who.cpp`)
- **realpath** — Print the resolved absolute pathname (`src/commands/realpath.cpp`)

## Remaining Missing Commands

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
- Prior art: `who.cpp` demonstrates utmp reading; `uptime.cpp` reads `/proc/uptime`.

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

## Testing Strategy

- Test at the command level via `tests/run_tests.sh` using `assert_cmd` / `assert_cmd_pat` helpers.
- For `pinky`: verify `pinky root` prints user information.
- For `stdbuf`: verify basic `--help` output and that invocation passes through to the command.

## Out of Scope

- `pinky` will not support the `-f` project file / `.plan` / `.project` file display on first implementation (read from home directory). These can be added later.
- `stdbuf` will not ship `libstdbuf.so`; it sets environment variables only. Full buffering control via `LD_PRELOAD` is a separate concern.
