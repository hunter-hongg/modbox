---
title: modbox-df
section: 1
date: 2026-08-19
author: modbox project
---

# NAME

modbox-df - report filesystem disk space usage

# SYNOPSIS

**modbox df** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Report disk space usage for each FILE (or the current directory if none specified).
Displays total, used, and available disk space in blocks or in human-readable format.

# OPTIONS

**-h**, **--human-readable**
:   Print sizes in human-readable format (e.g., 1K 234M 2G).

**-H**, **--si**
:   Like **-h**, but use powers of 1000 not 1024.

**-i**, **--inodes**
:   List inode information instead of block usage.

**-T**, **--type**
:   List type of filesystem.

**-a**, **--all**
:   Include dummy filesystems (type = tmpfs, devtmpfs, etc.).

**-B**, **--block-size=SIZE**
:   Scale sizes by SIZE before printing them. E.g., **-B M** prints sizes in megabytes.

**--output=FORMAT**
:   Use the output format defined by FORMAT. Currently, only the default POSIX output is supported.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Show disk usage in human-readable format
modbox df -h

# Show inode usage
modbox df -i

# Show filesystem type
modbox df -T

# Show usage for specific filesystem
modbox df /dev/sda1

# Show all filesystems including dummy ones
modbox df -a
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- The **-H** option uses SI units (1000-based), while **-h** uses binary units (1024-based).
- Block size defaults to 1K unless overridden by **-B** or the `BLOCK_SIZE` environment variable.
- Some columns may be abbreviated depending on terminal width.

# SEE ALSO

**modbox-du**(1), **modbox-mount**(1), **modbox**(1)
