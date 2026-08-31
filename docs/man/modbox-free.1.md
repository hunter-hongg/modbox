---
title: modbox-free
section: 1
date: 2026-08-29
author: modbox project
---

# NAME

modbox-free - display amount of free and used memory in the system

# SYNOPSIS

**modbox free** [*OPTION*]...

# DESCRIPTION

Display the amount of free and used memory in the system.

# OPTIONS

**-h**, **--human-readable**
:   Print sizes in human-readable format (e.g., `1K` `234M` `2G`).

**--si**
:   Like `-h`, but use powers of 1000 not 1024.

**-t**, **--total**
:   Show total row.

**-o**, **--old**
:   Use the old (legacy) format.

**--json**
:   Output in JSON format.

**--help**
:   Display help and exit.

**--version**
:   Output version information and exit.

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

modbox-vmstat(1), modbox(1)
