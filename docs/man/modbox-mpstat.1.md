---
title: modbox-mpstat
section: 1
date: 2026-08-29
author: modbox project
---

# NAME

modbox-mpstat - report CPU statistics

# SYNOPSIS

**modbox mpstat** [*OPTIONS*] [delay [count]]

# DESCRIPTION

Report CPU statistics, optionally per-CPU.

# OPTIONS

**-a**, **--all**
:   Report from all CPUs.

**-J**, **--json**
:   Output as JSON.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

delay
:   Delay between updates in seconds.

count
:   Number of updates to perform.

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

modbox-iostat(1), modbox-vmstat(1), modbox(1)
