# ModBox: GNU CoreUtils Compatibility - Missing Commands Implementation Overview

## Problem Statement

ModBox aims to be a comprehensive replacement for GNU Core Utilities. Currently, the following coreutils commands are missing from the implementation:

1. **hostname** — Get or set the system host name
2. **pathchk** — Check file names for portability and validity
3. **readlink** — Read symbolic links
4. **renice** — Change process scheduling priority (later)
5. **sha224sum** — SHA-224 checksum utility (later)
6. **wall** — Write to all logged-in users (later)

This document covers the implementation of the first three (`hostname`, `pathchk`, `readlink`), which form the foundational file system and environment inspection utilities.

## Solution Architecture

Each command will follow the existing ModBox command structure:

1. Header file in `include/commands/` declaring the command function.
2. Source file in `src/commands/` implementing the logic with argtable3 for option parsing.
3. Registration via `REGISTER_COMMAND("name", name_command, "help text")`.
4. Consistent error handling with `perror` and exit codes matching GNU behavior.
5. Complete test coverage added to `tests/`.

### Command Interface Summary

| Command | Short Options | Long Options | Primary Function |
|---------|--------------|--------------|------------------|
| hostname | `-a`, `-d`, `-f`, `-i`, `-I`, `-s`, `-p` | `--aliases`, `--domain`, `--fqdn`, `--addresses`, `--all-addresses`, `--short`, `precise` | Get/set host name |
| pathchk | `-p`, `-L`, `-n MAX`, `-w` | `--portability`, `--length`, `name-max=MAX`, `no-check-warnings` | Validate file names |
| readlink | `-f`, `-q`, `-s`, `-n` | `--canonicalize`, `--no-error`, `--strip`, `no-dereference` | Resolve symbolic links |

## Detailed Specifications

Individual specification documents have been created:

- **hostname**: `specs/hostname_spec.md` — Full spec covering all options, system calls, error cases, and test cases.
- **pathchk**: `specs/pathchk_spec.md` — Full spec covering POSIX validation rules, existence checks, and portability modes.
- **readlink**: `specs/readlink_spec.md` — Full spec covering symlink resolution, canonicalization, loop detection, and formatting options.

## Implementation Order

1. **readlink** — First (simplest, relies mainly on `lstat()`/`readlink()` system calls).
2. **hostname** — Second (requires `gethostname()`/`sethostname()` and DNS resolution logic).
3. **pathchk** — Third (most complex due to multiple validation modes and edge cases).

Rationale: Build confidence with simpler implementations before tackling more complex ones with extensive option combinations.

## Testing Strategy

All three commands will have corresponding test scripts under `tests/commands/`:

- `tests/commands/hostname.test.sh`
- `tests/commands/pathchk.test.sh`  
- `tests/commands/readlink.test.sh`

Tests will cover:
- Basic functionality verification
- Edge cases (null bytes, long paths, broken symlinks, loops)
- Option combinations
- Exit code correctness
- Error message format compliance
- Help and version output

The test runner script `tests/run_tests.sh` will aggregate all tests.

## Dependencies

- `argtable3` — For consistent argument parsing across all commands (already used throughout ModBox).
- System headers: `<unistd.h>`, `<sys/stat.h>`, `<errno.h>`, `<stdio.h>`, `<string.h>`, `<limits.h>`, `<netdb.h>` (for hostname), `<arpa/inet.h>` (optional for address conversion).
- No external libraries beyond standard libc and vcpkg-provided argtable3.

## Compliance Goals

- Command line interface compatibility with GNU coreutils 8.x/9.x.
- Exit codes match GNU behavior (0 for success, 1 for check failures, 2 for errors).
- Error messages use the format `command: FILE: reason` consistently.
- `--help` and `--version` output matches the modbox convention (GPLv3+ license notice).

## Next Steps

After these three commands are implemented and tested:

1. Implement remaining missing commands: `renice`, `sha224sum`, `wall`.
2. Run full test suite to ensure no regressions.
3. Update documentation (README, man pages if any) to reflect new commands.
4. Benchmark performance against GNU coreutils for critical paths.
