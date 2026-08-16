% MODBOX-FD(1) modbox | User Commands
% modbox project
% 2026-08-16

# NAME

modbox-fd - search for files in a directory hierarchy

# SYNOPSIS

**modbox fd** [*OPTION*]... **PATTERN** [*PATH*]...

# DESCRIPTION

Search for files matching **PATTERN** in a directory hierarchy.
Behavior is similar to the [fd-find](https://github.com/sharkdp/fd) tool.

The pattern is interpreted as a regular expression by default.
Use **-g** to switch to glob-based matching.
By default, hidden files and directories are excluded from the search.
The search is case-insensitive by default (smart-case).

# OPTIONS

## Matching control

**-s**, **--case-sensitive**
:   Force case-sensitive search.

**-i**, **--ignore-case**
:   Force case-insensitive search.

**-S**, **--smart-case**
:   Case-insensitive when the pattern is all lowercase (default behavior).

**-g**, **--glob**
:   Use glob-based search instead of regex.

**-p**, **--full-path**
:   Search the full path, not just the basename.

## Filtering

**-t**, **--type TYPE**
:   Filter by file type. TYPE is a single character:
    `f` (regular file), `d` (directory), `l` (symlink),
    `x` (executable), `e` (empty), `s` (socket).

**-e**, **--extension EXT**
:   Filter by file extension (e.g., `--extension txt` or `-e rs`).
    Can be specified multiple times.

**-E**, **--exclude PATTERN**
:   Exclude entries matching the given glob pattern.
    Can be specified multiple times.

**-H**, **--hidden**
:   Include hidden files and directories (those starting with `.`).

**-L**, **--follow**
:   Follow symbolic links into directories.

**-I**, **--no-ignore**
:   Do not respect `.gitignore` files.
    This is currently a no-op in modbox fd.

**-d**, **--max-depth DEPTH**
:   Descend at most DEPTH directories deep.
    A negative value means unlimited depth (default).

**--max-results NUM**
:   Stop after NUM matches.

## Output control

**-0**, **--print0**
:   Separate results by NUL characters instead of newlines.
    Useful for safely handling filenames with spaces or newlines.

**--color=WHEN**
:   Control color output. WHEN can be `always`, `auto` (default), or `never`.
    `--color` with no argument is treated as `--color=always`.

## Execution

**-x**, **--exec CMD**
:   Execute CMD for each search result.
    The placeholder `{}` is replaced with the matched file path.

**-X**, **--exec-batch CMD**
:   Execute CMD once with all search results as arguments.
    The placeholder `{}` is replaced with all matched paths.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Find all Rust source files
modbox fd --type f *.rs

# Search for files matching a regex pattern
modbox fd "test.*\.py$" /project

# Find hidden files
modbox fd --hidden "^\."

# Find executable files
modbox fd --type x

# Find empty files
modbox fd --type e

# Limit search depth to 2 levels
modbox fd --max-depth 2 src

# Execute a command for each result
modbox fd -x "echo {}"

# Print results NUL-separated for safe piping
modbox fd -0 | xargs -0 rm

# Glob-based search
modbox fd -g "*.log" /var/log

# Color output forced on
modbox fd --color=always "pattern"
```

# NOTES

- `.gitignore` files are not parsed; the `--no-ignore` flag is a no-op.
- Hidden files/directories are excluded by default; use `--hidden` to include them.
- For `--exec`, use `{}` to substitute the file path.
- The default smart-case behavior: patterns that are all lowercase are matched case-insensitively; patterns containing uppercase letters are matched case-sensitively.
- When output is not a terminal (e.g., piped), color is disabled unless `--color=always` is set.

# EXIT STATUS

`0`
:   At least one match was found.

`1`
:   No match was found.

`2`
:   An error occurred (e.g., invalid pattern, permission denied).

# SEE ALSO

**modbox-rg**(1), **modbox-ls**(1), **modbox-find**(1), **modbox**(1)
