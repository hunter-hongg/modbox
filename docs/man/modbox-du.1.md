---
title: modbox-du
section: 1
date: 2026-08-19
author: modbox project
---

# NAME

modbox-du - estimate file space usage

# SYNOPSIS

**modbox du** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Summarize disk usage of each FILE, recursively for directories.
Reports apparent or actual disk usage in the specified format.

# OPTIONS

**-a**, **--all**
:   Write counts for all files, not just directories.

**-b**, **--bytes**
:   Equivalent to `--apparent-size --block-size=1`.

**-c**, **--total**
:   Produce a grand total.

**-d**, **--max-depth=N**
:   Print the total summary for directories with at most N levels of depth.

**-h**, **--human-readable**
:   Print sizes in human-readable format (e.g., 1K 234M 2G).

**-H**, **--si**
:   Like **-h**, but use powers of 1000 not 1024.

**-k**
:   Like `--block-size=1K`.

**-m**
:   Like `--block-size=1M`.

**-S**, **--separate-dirs**
:   Do not include size of subdirectories.

**-s**, **--summarize**
:   Display only a total for each argument.

**-0**, **--null**
:   End each output line with NUL, not newline.

**-t**, **--threshold=SIZE**
:   Exclude entries smaller than SIZE, or greater than SIZE.

**--exclude=PATTERN**
:   Exclude files matching PATTERN.

**--apparent-size**
:   Print apparent sizes, not disk usage.

**--block-size=SIZE**
:   Scale sizes by SIZE before printing them.

**--time**
:   Show time of last modification.

**--count-links**
:   Count sizes multiple times if hard linked.

**-x**, **--one-file-system**
:   Skip directories on different filesystems.

**--json**
:   Output in JSON format.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Show disk usage of current directory
modbox du

# Human-readable format
modbox du -h

# Summary only for each argument
modbox du -sh *

# Maximum depth of 2 levels
modbox du -d 2

# Show all files including hidden
modbox du -a

# Grand total
modbox du -c /var/log
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- Without options, `du` shows apparent size in 1K blocks.
- The **-h** option uses binary units (1024-based), while **-H** uses SI units (1000-based).
- To see actual disk usage (including overhead), omit `--apparent-size`.

# SEE ALSO

**modbox-df**(1), **modbox**(1)
