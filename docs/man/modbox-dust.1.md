% MODBOX-DUST(1) modbox | User Commands
% modbox project
% 2026-08-02

# NAME

modbox-dust - disk usage with size bars

# SYNOPSIS

**modbox dust** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Disk usage with size bars, similar to the `dust` utility. Summarizes disk usage with a visual bar and sorts by size. With no FILE, reads the current directory. This command is an alias for `du --max-depth=1 -h` with additional formatting.

# OPTIONS

**-d**, **--depth**=*N*
:   Maximum depth to descend.

**-n**, **--number-of-lines**=*N*
:   Max lines to show, default 40. Use 0 for no limit.

**-a**, **--all**
:   Show all files, not just directories.

**-x**, **--one-file-system**
:   Skip directories on different filesystems.

**-H**, **--si**
:   Use powers of 1000 not 1024.

**-b**, **--bytes**
:   Show sizes in bytes.

**-c**, **--no-color**
:   Disable color output.

**-X**, **--exclude**=*PATTERN*
:   Exclude files matching PATTERN.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
modbox dust
modbox dust -d 2 -n 20
modbox dust -a -H
modbox dust -c /var/log
```

# NOTES

- Output is sorted by size descending.
- Bar width is fixed; color is disabled when stdout is not a TTY.

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

**modbox-du**(1), **modbox-df**(1), **modbox**(1)
