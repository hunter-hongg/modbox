# ModBox Implementation Spec: pathchk Command

## Problem Statement

The `pathchk` command checks file names for portability and validity. It verifies that file names are suitable for use on POSIX systems by checking length limits, character validity, and component size constraints. This command is part of GNU Core Utilities but is not currently implemented in ModBox, creating a gap for script portability testing.

## Solution

Implement the `pathchk` command that validates file names against POSIX.1 portability rules: total path length limit (255 characters), component name limit (255 characters), invalid null bytes, and optionally checks for existence and accessibility when requested.

## User Stories

1. As a script developer, I want to run `pathchk FILE` so that it reports if a file name would be portable across POSIX systems.
2. As a quality engineer, I want to run `pathchk -p` so that only strict POSIX.1 portability checks are performed (no existence/access checks).
3. As a developer using Long File Name extensions, I want to run `pathchk -L` to allow longer names up to 4095 characters.
4. As a security auditor, I want to run `pathchk -n MAX` to specify maximum component name length.
5. As a developer, I want to run `pathchk -p -w` to check for portability without validating file existence or accessibility.
6. As a user debugging filename issues, I want to run `pathchk long/path/with/very/deep/nested/file.txt` to detect if any component exceeds system limits.
7. As a CI script author, I expect `pathchk` to return exit code 0 if all files pass checks, non-zero if any check fails.
8. As a tester, I want `pathchk` to reject filenames containing null bytes (`\0`).
9. As an internationalization specialist, I want `pathchk` to validate multibyte character sequences are valid in the current locale.
10. As a cross-platform developer, I want `pathchk` with `-p` to enforce only the mandatory POSIX.1 restrictions (255 char limit, no leading `-`, etc.).
11. As a build system integrator, I want `pathchk` to handle multiple files: `pathchk file1 file2 file3` and report each individually.
12. As a user working on Windows WSL, I expect `pathchk` to understand Linux-style paths while respecting the underlying filesystem's limits.
13. As a security tool author, I want `pathchk --help` to display usage information.
14. As a versioned tool consumer, I want `pathchk --version` to show version info.
15. As a robust script writer, I expect pathchk to gracefully handle non-existent files when `-p` is used (skip existence check).

## Implementation Decisions

### Command Interface

- Syntax: `pathchk [OPTION]... FILE...`
- Files can be regular files, directories, symlinks, etc. The command validates the path string itself, not necessarily the actual filesystem object (unless existence checks are enabled via default behavior without `-p`).
- Multiple FILE arguments are processed sequentially; each produces its own diagnostic output if invalid.

### Option Set (matching GNU coreutils)

| Option | Description |
|--------|-------------|
| `-p`, `--portability` | Check that the file names are POSIX.1 portable; do not check existence or accessibility |
| `-L`, `--length` | Allow up to 4096-character names (instead of the standard 255) |
| `-n MAX`, `name-max=MAX` | Assume file names have at most MAX characters (default 255, or 4095 with `-L`) |
| `-w`, `no-check-warnings` | Do not warn about potentially problematic file names (e.g., starting with `-`) |
| `--help` | Display help and exit successfully |
| `--version` | Output version information and exit successfully |

**Note**: GNU coreutils also supports `-P`, `--physical` (don't follow symlinks), `-X`, `--extreme` (don't warn about slashes); these may be added later.

### Validation Rules (POSIX.1)

For each file path provided:

1. **Null byte check**: Reject any path containing a `\0` character (not allowed in POSIX file names).
2. **Component length**: Split the path into components (by `/`). For each component:
   - If `-p` is specified: component length must be ≤ `MAX` (default 255, or 4095 with `-L`)
   - If `-p` is NOT specified: component length must be ≤ `NAME_MAX` for the filesystem where the component resides (may require `fstatat()` or similar)
3. **Total path length**: If `-p` is specified: entire path length (excluding trailing slash if present) must be ≤ `PATH_MAX` (typically 4096, or 255 without `-L`). Default `PATH_MAX` = 256 without `-L`, 4097 with `-L`.
4. **Leading slash**: Paths starting with `/` are absolute; must still respect component lengths.
5. **Component name `-`**: A component named exactly `-` could be interpreted as a command-line option; warn unless `-w` is set.
6. **Empty component**: Consecutive slashes produce empty components which are invalid; reject.
7. **Unicode/multibyte**: In locales with multibyte characters, ensure byte count per component doesn't exceed limit (count bytes, not characters).

### Existence and Accessibility Checks (when `-p` NOT used)

When `-p` is not specified, additional checks are performed:

1. **Existence**: Verify the file or directory actually exists (for symlink targets, follow by default).
2. **Readability**: Check read permission on the file/directory (for directories, check search/execute permission on all parent components).
3. **Writeability**: Not required by default, but can be extended.

These checks use `access()` or `stat()` system calls; failures produce diagnostic messages like "pathchk: filename: No such file or directory".

### Error Messages Format

For validation failures (without `-p`):
```
pathchk: FILE: Reason message
```

Examples:
- `pathchk: my/file/name: File name too long`
- `pathchk: /a/b/c: Name too long`
- `pathchk: file-with-dash-start: Warning: file name starts with '- ' (portability issue)`

Exit status:
- `0` — all files passed all checks
- `1` — at least one file failed a check
- `2` — invalid command line options or other errors

### File Structure

- Header: `include/commands/pathchk.hpp` — declares `pathchk_command(int argc, char** argv)`
- Source: `src/commands/pathchk.cpp` — implements parsing, validation logic, and registration via `REGISTER_COMMAND("pathchk", pathchk_command, "Check file names for validity and portability")`

### Versioning

Same as other commands:
```
pathchk (modbox) 1.0
Copyright (C) 2026 modbox
License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>
```

## Testing Decisions

### Test Approach

Use `tests/run_tests.sh` following existing patterns. Test both successful passes and failure conditions, verifying exit codes and stderr output.

### Test Cases to Implement

1. **Simple valid file**: `pathchk valid_file.txt` exits 0, no output.
2. **Long component test**: Create a file with component > 255 chars; `pathchk` should fail with "File name too long".
3. **Long total path test**: Deeply nested path exceeding PATH_MAX; should fail.
4. **-p flag**: `pathchk -p very_long_name` should check only length limits without requiring file existence.
5. **-L flag**: `pathchk -p -L超长文件名` (up to 4095 chars) should pass.
6. **-n option**: `pathchk -n 10 short` with component ≤ 10 passes; with longer fails.
7. **Null byte rejection**: Path containing `\0` should be rejected immediately.
8. **Leading dash warning**: `pathchk -w -file` suppresses warning; without `-w` prints warning.
9. **Multiple files**: `pathchk good.txt bad/file.txt` processes both, reports separately.
10. **Non-existent file with -p**: `pathchk -p /nonexistent/path` should not error on missing file, only on portability.
11. **Non-existent file without -p**: Should report "No such file or directory".
12. **Directory with spaces**: `pathchk "my dir/file.txt"` handles quoted paths correctly.
13. **Symlink target**: When following symlinks, validate the target path as well (or just the link path depending on implementation).
14. **Help output**: `--help` displays usage, exits 0.
15. **Version output**: `--version` shows version string, exits 0.
16. **Invalid option**: Unknown option prints error, exits non-zero.
17. **Empty filename**: `pathchk ""` or `pathchk ""` should handle edge case gracefully.

### Existing Test Patterns

Refer to tests in `tests/` directory for pattern matching on command output and exit codes. Similar to how `test` or `[` command tests are structured.

## Out of Scope

- Filesystem-specific `NAME_MAX`/`PATH_MAX` lookup via `pathconf()` when `-p` is not used (simple static constants acceptable for initial implementation).
- `-P` (don't follow symlinks) and `-X` (extreme warnings) options — can be added in future iterations.
- Windows compatibility layer (initially target POSIX-like systems).
- Internationalized error messages (all messages in English/POSIX locale).

## Further Notes

- The `pathchk` utility from GNU coreutils is primarily a development aid; it doesn't need full filesystem semantics, just basic string validation plus optional existence checks.
- Be careful with byte vs character counting in multibyte locales (use `mbrlen` or similar if needed). For simplicity, count bytes initially.
- For `-n MAX`, ensure the value is validated (positive integer, within reasonable bounds).
- Consider whether to implement `-L` as allowing up to 4095 or simply disabling the length check entirely (GNU coreutils uses 4096 for component, 4097 for total path).
