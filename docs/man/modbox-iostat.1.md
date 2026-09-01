% MODBOX-IOSTAT(1) modbox | User Commands
% modbox project
% 2026-08-29

# NAME

modbox-iostat - report CPU and I/O statistics

# SYNOPSIS

**modbox iostat** [*OPTION*]...

# DESCRIPTION

Display CPU and I/O statistics.

# OPTIONS

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

**-J**, **--json**
:   Output in JSON format.

**-S**, **--unit**
:   Byte scaling (`K`, `M`, `G`; default auto-sector).

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

modbox-vmstat(1), modbox-mpstat(1), modbox(1)
