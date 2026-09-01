% MODBOX-VMSTAT(1) modbox | User Commands
% modbox project
% 2026-08-29

# NAME

modbox-vmstat - display system performance statistics

# SYNOPSIS

**modbox vmstat** [*OPTION*] [delay [count]]

# DESCRIPTION

Display system performance statistics, including memory, swap, I/O and CPU.

# OPTIONS

**-a**
:   Display active/inactive memory fields.

**-d**
:   Display disk statistics.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

**delay**
:   Delay between updates in seconds.

**count**
:   Number of updates to perform.

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

modbox-iostat(1), modbox-free(1), modbox(1)
