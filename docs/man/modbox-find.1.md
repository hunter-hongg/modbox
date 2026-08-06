% MODBOX-FIND(1) modbox | User Commands
% modbox project
% 2026-08-06

# NAME

modbox-find - search for files in a directory hierarchy

# SYNOPSIS

**modbox find** [*starting-point*]... [\-**name* *PATTERN*]...

# DESCRIPTION

**find** walks a directory tree and evaluates a user-supplied expression
(constructed from *predicates* and *actions*) for each file it encounters.
When no starting point is given, the current directory `.` is used.
When no action is given, **-print** is assumed.

# MODES

**--tui**
:   Open an interactive TUI viewer for the search results.
    Requires a TTY; falls back to normal output when stdout is not a terminal.
    This is a modbox extension not found in standard find.

**--json**
:   Output results in JSON format instead of plain text paths.
    This is a modbox extension not found in standard find.

# PREDICATES

**-name PATTERN**
:   True if the file name (basename) matches the shell glob *PATTERN*.
    `*` matches any string, `?` matches any single character.

**-iname PATTERN**
:   Like **-name**, but the match is case-insensitive.

**-type TYPE**
:   True if the file type matches *TYPE*.
    `f` = regular file, `d` = directory, `l` = symbolic link.

**-empty**
:   True if the file is empty.
    Regular files with size 0 and directories containing no entries
    (other than `.` and `..`) are considered empty.

# NUMERIC OPTIONS

**-maxdepth N**
:   Descend at most *N* levels below the starting points.
    A starting point is at depth 0; its children are at depth 1, etc.
    `0` tests only the starting points themselves.

**-mindepth N**
:   Do not apply tests or actions at levels less than *N*.
    Like **-maxdepth** but prevents descending too shallowly.

# ACTIONS

**-print**
:   Print the full file path followed by a newline.
    This is the default action when no action is specified.

**-delete**
:   Delete the current file or empty directory.
    For directories, only empty directories can be deleted (via `rmdir`).
    For regular files, `unlink` is used.

**-exec CMD **{}** ;**
:   Execute *CMD* once for each matching file.
    The string **{}** is replaced by the current file path.
    A semicolon (`;`) terminates the command.

**-exec CMD **{}** +**
:   Execute *CMD* once, passing all matching file paths as arguments.
    The string **{}** is replaced by a batch of file paths.
    A plus sign (`+`) terminates the command.
    This is more efficient than `;` when many files match.

# DEFAULT BEHAVIOR

- If no starting point is specified, `.` (current directory) is used.
- If no action is specified, **-print** is assumed.
- When no predicates are given, all files in the tree match.
- The **-delete** action implies **-depth** (process directory contents
  before the directory itself).

# EXIT STATUS

`0`
:   Successful execution.

non-zero
:   An error occurred (e.g., permission denied on a directory).

# NOTES

- **-maxdepth** and **-mindepth** are relative to the starting points,
  not the filesystem root.
- **-exec ... ;** spawns one process per matching file; use **-exec ... +**
  for better performance on large result sets.
- The order in which files are visited follows the directory entry order.
- Symlinks are not followed by default (use **-H**, **-L**, or
  **-follow** for symlink traversal; these are not implemented in modbox).
- **-empty** on a directory checks whether it contains any entries
  other than `.` and `..`.

## Differences from GNU find

Not implemented: `-size`, `-mtime`, `-newer`, `-perm`, `-user`, `-group`,
`-regex`, `-ipath`, `-prune`, `-delete` with non-empty directories,
`-execdir`, `-ok`, `-ls`, `-printf`, `-fprintf`, `-path`, `-iname`
(with glob variants), `-links`, `-context`, `-xtype`.

# EXAMPLES

```bash
# Find all C source files in the current directory tree
modbox find -name "*.c"

# Find regular text files under a directory
modbox find . -type f -name "*.txt"

# Delete empty files and directories under /tmp
modbox find /tmp -empty -delete

# Run a command on each matching file
modbox find . -type f -exec wc -l {} ;

# Batch execution (more efficient)
modbox find . -name "*.log" -exec rm {} +

# Limit search depth to 2 levels
modbox find -maxdepth 2 -type d -name "test*"

# Output results as JSON
modbox find . --json

# Open interactive TUI viewer
modbox find . --tui
```

# SEE ALSO

**modbox-grep**(1), **modbox-sed**(1), **modbox-awk**(1), **modbox**(1)
