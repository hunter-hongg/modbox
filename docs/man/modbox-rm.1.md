% MODBOX-RM(1) modbox | User Commands
% modbox project
% 2026-08-02

# NAME

modbox-rm - remove files or directories

# SYNOPSIS

**modbox rm** [*OPTION*]... *FILE*...

# DESCRIPTION

Remove (unlink) the FILE(s).

# OPTIONS

`-d`, `--dir`
:   Remove empty directories.

`-f`, `--force`
:   Ignore nonexistent files and arguments, never prompt.

`-i`, `--interactive`
:   Prompt before every removal.

`-r`, `--recursive`
:   Remove directories and their contents recursively.

`-v`, `--verbose`
:   Explain what is being done.

`--one-file-system`
:   When removing a hierarchy recursively, skip any directory that is on a file system different from that of the corresponding command line argument.

`--no-preserve-root`
:   Do not treat `/` specially.

`--preserve-root`
:   Do not remove `/` (default).

`--trash`
:   Move files to `~/.trash` instead of deleting.

`-h`, `--help`
:   Display help and exit.

# EXAMPLES

```bash
modbox rm file.txt
modbox rm -r dir/
modbox rm -rf dir/
modbox rm -v file.txt
modbox rm -i file.txt
modbox rm -d emptydir/
modbox rm --trash file.txt
modbox rm --trash -r dir/
```

# NOTES

- `-f` overrides `-i` (if both are given, `-f` takes precedence).
- By default, `rm` does not remove directories. Use `-r` or `-d` for directories.
- The `--trash` option moves files to `~/.trash` instead of permanently deleting them. This is a modbox extension.
- `--preserve-root` is the default; `--no-preserve-root` is dangerous and not recommended.
- When using `--one-file-system`, directories on different filesystems are skipped during recursive removal.
- The `--trash` option creates the trash directory `~/.trash` if it doesn't exist.

# EXIT STATUS

`0` on success, `1` on error.

# SEE ALSO

**modbox-cat**(1), **modbox-ls**(1), **modbox**(1)