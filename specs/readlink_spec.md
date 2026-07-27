# ModBox Implementation Spec: readlink Command

## Problem Statement

The `readlink` command reads symbolic links and outputs their target path. It is commonly used in shell scripts to resolve symlinks to their canonical paths. This command is part of GNU Core Utilities but is not currently implemented in ModBox, creating a gap for symlink resolution functionality.

## Solution

Implement the `readlink` command that follows the GNU specification: given a file, print its symbolic link target (if it is a symlink), or if given a non-symlink, optionally resolve the canonical path with various options (-f, -q, -s, -n).

## User Stories

1. As a script writer, I want to run `readlink symlink` so that the target path of a symbolic link is printed to stdout.
2. As a developer, I want to run `readlink -f PATH` so that all symbolic links in the path are resolved recursively, producing the canonical absolute path.
3. As an automation tool author, I want `readlink -q` to suppress error messages when the file is not a symlink (quiet mode).
4. As a user parsing output, I want `readlink -s` to print the symlink string including surrounding parentheses (like ls -l shows).
5. As a user controlling newline behavior, I want `readlink -n` to disable trailing newline output.
6. As a cross-platform scripter, I expect `readlink` to handle both relative and absolute symlinks correctly, preserving the literal target as stored in the symlink (unless `-f` resolves it).
7. As a pipeline operator, I want `readlink` to work properly when chained with other commands: `readlink file | xargs`.
8. As a security auditor, I want `readlink -f` to resolve through multiple levels of nested symlinks until reaching a non-symlink or loop detection triggers.
9. As a user dealing with broken symlinks, I want `readlink -f` to output as much as possible before the first broken link, or fail gracefully.
10. As a user testing, I want `readlink --help` to display usage information.
11. As a versioned tool consumer, I want `readlink --version` to show version info.
12. As a script handling multiple inputs, I want to pass multiple files: `readlink link1 link2` and get each on separate lines.
13. As a user working with stdin, I expect `readlink -` to read from standard input (path to resolve).
14. As a robust script author, I want `readlink` to detect symlink loops and report them rather than looping infinitely.
15. As a user wanting just the basename of a resolved path, I plan to combine `readlink -f file | xargs basename`.

## Implementation Decisions

### Command Interface

- Syntax: `readlink [OPTION]... FILE...`
- If no FILE is given, or FILE is `-`, read the path from standard input (one path per line).
- Multiple files are processed independently; each produces its own output.

### Option Set (matching GNU coreutils)

| Option | Description |
|--------|-------------|
| `-f`, `--canonicalize` | Canonicalize by resolving every symbol in the path to an absolute path; following symlinks until the last one which must exist |
| `-q`, `--no-error` | Suppress error messages for non-symlinks or inaccessible files |
| `-s`, `--strip` | Strip trailing whitespace and do not put parentheses around the symlink output (GNU default without -f); actually `-s` in GNU means "don't add parentheses" — need to verify exact semantics |
| `-n`, `--no-dereference` | Do not add trailing newline (when used with `-f`) |
| `--help` | Display help and exit successfully |
| `--version` | Output version information and exit successfully |

**Clarification on `-s`**: In GNU coreutils `readlink`:
- Without `-f`: default output includes parentheses around the symlink target if terminal is a tty (for compatibility with `ls -l`). The `-s` option strips those parentheses and also removes trailing spaces.
- With `-f`: `-s` behaves like strip whitespace/no-newline.

Actually checking GNU documentation: `-s, --strip` strip trailing whitespace and do not put parentheses around the output. When used with `-f`, `-n` disables trailing newline.

Let me refine:

Without `-f`:
- Default: output is `(target)` when writing to terminal, or just `target` otherwise.
- `-s`: always strip parentheses and trailing spaces.

With `-f`:
- Output is the canonicalized absolute path.
- `-n`: do not output trailing newline.
- `-s`: strip trailing whitespace (same effect).

### File Structure

- Header: `include/commands/readlink.hpp` — declares `readlink_command(int argc, char** argv)`
- Source: `src/commands/readlink.cpp` — implements symlink reading, canonicalization, and registration via `REGISTER_COMMAND("readlink", readlink_command, "Print target of a symbolic link")`

### Algorithm Details

#### Basic Symlink Reading (without -f)

1. For each file argument:
   a. If file is `"-"`, read a line from stdin.
   b. Use `lstat()` instead of `stat()` to avoid following the symlink.
   c. Check if the file is a symbolic link using `S_ISLNK(st_mode)`.
   d. If not a symlink:
      - If `-q` flag set, silently skip (exit 0 for that file).
      - Else, print `readlink: FILE: Not a symbolic link` to stderr.
   e. If it is a symlink, use `readlink(buffer, bufsize)` to get the target string.
   f. Apply `-s` formatting: remove leading/trailing whitespace, strip parentheses if present.
   g. Output the result followed by newline (unless `-n` used with `-f`).

#### Canonicalization (`-f` option)

Algorithm inspired by `glibc realpath()` but with custom implementation:

1. Start with the input path (absolute or relative).
2. Resolve each component iteratively:
   a. If current component is a symlink, read its target.
   b. If the symlink target is absolute (`/` prefix), reset current path to the target's root, then process remaining components.
   c. If the symlink target is relative, append it to the current directory's parent directory.
   d. Continue walking through the rest of the path components.
3. Handle edge cases:
   - Broken symlink: output what was resolved so far (or fail entirely depending on strictness).
   - Symlink loop: detect cycles by tracking visited inode numbers or path strings, limit recursion depth (e.g., 50 levels), and report error.
   - Permission denied: if any directory lacks execute permission, fail unless `-q` is set.
4. Final path should be normalized: eliminate `.` and `..` components, resolve redundant slashes.

**Note**: Implementing full `realpath`-style canonicalization from scratch is complex. Consider using system-provided `realpath()` (POSIX.1-2001) if available, wrapping it with symlink loop detection and error handling. However, `realpath()` may not distinguish between `-f` behavior (follow symlinks vs. just canonicalizing path string) precisely. A custom iterative solution gives more control.

### Error Messages Format

```
readlink: FILE: Reason
```

Examples:
- `readlink: link1: No such file or directory`
- `readlink: loop_link: Too many levels of symbolic links`
- `readlink: inaccessible_dir: Permission denied`

Exit status:
- `0` — all files processed successfully (or with `-q` even if some weren't symlinks)
- `1` — at least one file had an error and `-q` was not specified
- `2` — invalid command line options or other errors

### File Structure

- Header: `include/commands/readlink.hpp`
- Source: `src/commands/readlink.cpp`
- Registration: `REGISTER_COMMAND("readlink", readlink_command, "Print target of a symbolic link")`

### Versioning

```
readlink (modbox) 1.0
Copyright (C) 2026 modbox
License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>
```

## Testing Decisions

### Test Approach

Use `tests/run_tests.sh` with assertions on output, exit codes, and stderr. Test both basic symlink resolution and advanced `-f` canonicalization. Create test fixtures with various symlink configurations.

### Test Cases to Implement

1. **Basic symlink**: Create a symlink `ln -s target link`; `readlink link` outputs `target`.
2. **Relative symlink**: `ln -s ../dir/target link`; `readlink link` outputs the raw relative target.
3. **-s option with parentheses**: Ensure `-s` strips parentheses from output (when output would normally include them for terminal).
4. **Broken symlink**: `readlink broken_link` reports error (without `-q`), exits 1.
5. **Non-file**: `readlink regular_file` (not a symlink) — report error unless `-q`.
6. **-q mode**: `readlink -q non_symlink` exits 0 with no stderr output.
7. **-f on symlink**: `readlink -f link` resolves through symlink(s) to the actual file's absolute path.
8. **-f on existing directory**: `readlink -f dir` resolves to the directory's absolute path (even though it's not a symlink).
9. **Multiple symlinks**: Chain of symlinks (a→b→c); `readlink -f a` resolves to final target c.
10. **Symlink loop detection**: Create circular symlink; `readlink -f` detects loop and reports error.
11. **-n option**: `readlink -f -n link` outputs without trailing newline (verify by checking output length/hex).
12. **stdin input**: `echo symlink_path | readlink -` resolves the stdin-provided path.
13. **Multiple files**: `readlink link1 link2` processes both sequentially.
14. **Help output**: `--help` displays usage, exits 0.
15. **Version output**: `--version` shows version string, exits 0.
16. **Invalid options**: Unknown option prints error, exits non-zero.
17. **Path with spaces**: `readlink "link with spaces"` handles correctly.
18. **Nested -f and -s**: Combination works as expected.

### Existing Test Patterns

Look at how similar path-handling commands are tested (`basename`, `dirname`, `realpath` if exists). Check test fixtures directory for setup/cleanup patterns.

## Out of Scope

- Windows-specific symlink handling (initially target POSIX-like systems only).
- `-p`, `--physical` (don't follow symlinks) and `-H`, `-L` (follow or don't follow symlinks during traversal) — these are variations from some implementations; GNU doesn't have them.
- Integration with `ls -l` style automatic parenthesis addition (handled internally based on whether output is to tty).
- Performance optimization for very deep directory trees (correctness first).

## Further Notes

- The `readlink` utility appears in both `coreutils` (as a standalone) and also in `findutils` (older versions); ensure we match the GNU coreutils version behavior.
- For the `-f` option, the key distinction from `realpath(3)` is that `readlink -f` requires the final component after resolving all symlinks to actually exist (unlike `realpath` which can return a path for a non-existent file). If the final target doesn't exist, `readlink -f` should fail.
- Detecting symlink loops: Track visited inodes (st_ino + st_dev) during traversal, or track resolved path strings as a simple alternative. Limit maximum iterations (e.g., 100) to prevent hangs.
- For absolute path determination: If starting with a relative path, `readlink -f` first needs to `chdir` to the containing directory or use `getcwd()` + prepend, then resolve. Alternatively, use `/proc/self/cwd` on Linux.
- Consider using `realpath()` for the core algorithm and wrap it with additional symlink-following logic to achieve strict `readlink -f` behavior (where all symlinks must be resolved and the final file must exist).
