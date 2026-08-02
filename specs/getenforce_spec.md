# ModBox Implementation Spec: getenforce Command

## Problem Statement

The `getenforce` command reports whether SELinux is currently running in enforcing, permissive, or disabled mode. It is part of the reference policy SELinux utilities but is not currently implemented in ModBox, creating a gap for users who expect a consistent set of common system administration tools in a single binary.

## Solution

Implement the `getenforce` command that follows the GNU refpolicy `getenforce` specification exactly: no arguments required, outputs a single word to stdout — `Enforcing`, `Permissive`, or `Disabled`.

## User Stories

1. As a system administrator, I want to run `getenforce` with no options so that the current SELinux enforcement mode is printed to stdout.
2. As a script writer, I want `getenforce` to output exactly `Enforcing`, `Permissive`, or `Disabled` (with a trailing newline) so that shell conditionals like `[ "$(getenforce)" = "Enforcing" ]` work reliably.
3. As a security engineer, I want `getenforce` to read the live kernel enforcement state via the SELinux ABI so that the output reflects the actual runtime configuration.
4. As a user on a system without SELinux support compiled into the kernel, I want `getenforce` to output `Disabled` rather than crashing or producing an error.
5. As a developer, I want `getenforce --help` to display concise usage information and exit successfully.
6. As a developer, I want `getenforce --version` to display version information consistent with other modbox commands.
7. As a test author, I want the command to have zero positional arguments accepted so that passing unexpected arguments produces a clear error.
8. As a sysadmin auditing a box, I want `getenforce` to be fast and lightweight since it only reads a kernel interface and prints a string.

## Implementation Decisions

### Command Interface

- **Arguments**: No positional arguments accepted. Any non-option argument is an error.
- **Options**: Only `--help` and `--version` are supported. No other flags modify behavior.
- **Output**: A single line written to stdout, followed by a newline character.
- **Exit codes**: 0 on success; non-zero on invalid option or parse error.

### Option Set

| Option | Description |
|--------|-------------|
| `--help` | Display usage information and exit successfully |
| `--version` | Display version string and exit successfully |

All other options are rejected as errors (matching GNU refpolicy behavior).

### System Call / Read Path

The implementation uses the libselinux C library which is already a project dependency (used by `chcon` and `runcon`). Two approaches are possible; we select the simpler and more direct one:

1. **Preferred**: Call `security_getenforce()` from `<selinux/selinux.h>`.
   - Returns `1` → print `Enforcing`
   - Returns `0` → print `Permissive`
   - Returns `-1` → SELinux is not active / not compiled in → print `Disabled`
   
   This is the canonical API and matches how the reference implementation works.

2. **Fallback** (if `security_getenforce()` is unavailable): Read `/sys/fs/selinux/enforce` directly:
   - Content `1` → `Enforcing`
   - Content `0` → `Permissive`
   - File missing/read error → `Disabled`

We implement the fallback only if compilation against libselinux fails, but given that libselinux is already in `PKGS` in the Makefile, the primary path will always be available.

### Error Handling

- Invalid/unrecognized option: print error to stderr matching GNU refpolicy style (`getenforce: unrecognized option '--xxx'`) and exit non-zero.
- Unknown positional argument: print usage hint to stderr and exit non-zero.
- SELinux kernel interface unavailable and fallback path also unavailable: print `Disabled` to stdout (graceful degradation, matching GNU behavior on unsupported systems).

### File Structure

- Header: `include/commands/getenforce.hpp` — declares `int getenforce_command(int argc, char** argv);`
- Source: `src/commands/getenforce.cpp` — implements the command with argtable3 parsing, `security_getenforce()` call, and registration via `REGISTER_COMMAND("getenforce", getenforce_command, "Print the current SELinux mode")`
- Registration occurs in `src/main.cpp` via insertion into the commands map (or via the `REGISTER_COMMAND` macro which auto-registers at static init time)

### Version Format

Uses the shared `print_version()` utility (see `include/commands/version_util.hpp`), consistent with all other modbox commands:

```
getenforce (modbox) 1.0
```

## Testing Decisions

### Test Approach

Use the existing test framework at `tests/run_tests.sh`. Follow the pattern of simple bash assertions checking command output against expected strings. See existing tests for similar simple commands like `uname`, `hostid`, `logname`.

### Test Cases to Implement

1. **Basic output when SELinux is enforcing**: Run `getenforce` and verify output matches the system's current mode (use `$(/usr/sbin/getenforce)` as ground truth on SELinux-enabled systems; on non-SELinux systems, expect `Disabled`).
2. **Output format**: The output must be exactly one of `Enforcing`, `Permissive`, or `Disabled` — nothing else on stdout.
3. **--help**: `assert_cmd_pat 'Usage:' getenforce --help`
4. **--version**: `assert_cmd_pat 'getenforce \(modbox\) 1\.0' getenforce --version`
5. **No positional arguments accepted**: Pass an unexpected argument and verify it produces an error on stderr (`assert_cmd_pat_stderr 'unexpected argument' getenforce foo`).
6. **Unknown option rejected**: Pass `--foo` and verify stderr contains `unrecognized option` (`assert_cmd_pat_stderr 'unrecognized option' getenforce --foo`).
7. **Exit code on success**: Verify exit code is 0 for valid invocations.
8. **Exit code on error**: Verify exit code is non-zero for invalid invocations.

### Conditional Test Execution

Because SELinux may not be present on all CI machines, tests for the actual enforcement mode value should gracefully handle the case where `/usr/sbin/getenforce` is unavailable. In that case, skip the ground-truth comparison and instead verify only that the output is one of the three valid tokens.

### Existing Test Patterns

Examine `tests/test_hostid.sh` and `tests/test_uname.sh` for the simplest assertion patterns. The key helper functions are:
- `assert_cmd EXPECTED actual args...` — exact stdout match
- `assert_cmd_pat PATTERN args...` — regex match on stdout
- `assert_cmd_pat_stderr PATTERN args...` — regex match on stderr

## Out of Scope

- Setting the enforcement mode (`setenforce` command) — this is a separate utility with different privilege requirements.
- Parsing `/etc/selinux/config` for the configured policy mode — only the live kernel state matters.
- Processing `-V` short form (GNU refpolicy does not support this; we match that).
- Any TUI or interactive mode.
- Cross-platform support beyond Linux with libselinux — initial implementation targets Linux only.

## Further Notes

- The GNU refpolicy `getenforce` is intentionally minimal: no options beyond `--help` and `--version`. This spec follows that design.
- `security_getenforce()` is part of libselinux >= 2.0, which is already a build dependency. No additional linking is required.
- On kernels without SELinux compiled in, `security_getenforce()` returns `-1`; the implementation should treat this as `Disabled`, not as an error.
- The command should be added to the README.md command list alongside existing SELinux-related commands (`chcon`, `runcon`).
