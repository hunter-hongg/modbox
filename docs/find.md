# modbox `find` Command

Search for files in a directory hierarchy. A minimal POSIX-style `find` with
common predicates for filtering by name, type, depth, and emptiness, plus
`-exec` for running commands on results.

## Synopsis

```
find [starting-point...] [expression]
```

With no starting points, the current directory `.` is used. The expression is
composed of predicates (tests) and actions.

## Predicates (Tests)

| Predicate           | Description |
|---------------------|-------------|
| `-name PATTERN`     | Shell glob pattern matching on filename |
| `-iname PATTERN`    | Case-insensitive `-name` |
| `-type [fdl]`       | File type: `f` (regular), `d` (directory), `l` (symlink) |
| `-empty`            | File is empty (regular file of size 0, or directory with no entries) |

## Numeric Options

| Option           | Description |
|------------------|-------------|
| `-maxdepth N`    | Descend at most N levels below starting points (0 = only starting points) |
| `-mindepth N`    | Do not apply tests/actions at levels less than N |

## Actions

| Action                | Description |
|-----------------------|-------------|
| `-print`              | Print full path of each matching file (default if no action specified) |
| `-delete`             | Delete matching files or empty directories |
| `-exec cmd {} ;`      | Execute command on each matching file (`{}` replaced with file path) |
| `-exec cmd {} +`      | Execute command with all matching files appended at once |

If no action is given, `-print` is assumed.

### Behavior

- Predicates are ANDed: a file must match all specified predicates.
- `-maxdepth` is checked before descending into subdirectories.
- `-mindepth` skips evaluation of entries shallower than N.
- `-delete` uses `unlink()` for files and `rmdir()` for directories (only
  empty directories can be removed).
- `-exec {} ;` forks a child process per matching file.
- `-exec {} +` accumulates all matching paths and runs one command.
- Starting points themselves are evaluated (depth 0); with `-maxdepth 0`,
  only the starting point is processed.
- Symlinks are not dereferenced by default; `-type l` matches symlinks.

## Examples

```bash
# Find all C source files
find . -name "*.c"

# Find only regular files with .txt extension
find /tmp -type f -name "*.txt"

# Find empty directories
find . -type d -empty

# Delete all .log files in /tmp (recursive)
find /tmp -name "*.log" -delete

# Count lines in each .c file
find . -name "*.c" -exec wc -l {} ;

# Remove all .o files in one go
find build -name "*.o" -exec rm {} +

# Search at most 2 levels deep, case-insensitive
find /usr/include -maxdepth 2 -iname "*.h"

# Skip the starting directory itself
find . -mindepth 1 -name "*.py"
```
