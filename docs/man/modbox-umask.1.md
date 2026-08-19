---
title: modbox-umask
section: 1
date: 2026-08-19
author: modbox project
---

# NAME

modbox-umask - set file creation mask

# SYNOPSIS

**modbox umask** [**-S**] [**-p**] [**MASK**]

# DESCRIPTION

Display or set the file creation mask (umask).
The umask controls the initial file permissions for newly created files and directories.

# OPTIONS

**-S**, **--symbolic**
:   Use symbolic form instead of octal (e.g., `u=rwx,g=rx,o=`).

**-p**, **--print**
:   Output in a reusable form that can be sourced by the shell.
    Output format: `umask 0022`

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# MASK FORMAT

**MASK** can be specified in two formats:
- **Octal**: Three or four octal digits (e.g., `022`, `0022`)
- **Symbolic**: Permission combinations like `u=rwx,g=rx,o=`

# EXAMPLES

```bash
# Show current umask
modbox umask

# Show in symbolic form
modbox umask -S

# Set umask to 022
modbox umask 022

# Set umask to 077 (more restrictive)
modbox umask 077

# Print in reusable format
eval "$(modbox umask -p)"
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- The umask value is subtracted from the default permissions (666 for files, 777 for directories).
- Common values:
  - `022`: Files get 644, directories get 755
  - `027`: Files get 640, directories get 750
  - `077`: Files get 600, directories get 700
- The umask persists for the current shell session.

# SEE ALSO

**modbox-chmod**(1), **modbox-stat**(1), **modbox-env**(1)
