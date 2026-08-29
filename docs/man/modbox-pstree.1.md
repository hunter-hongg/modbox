% MODBOX-PSTREE(1) modbox | User Commands
% modbox project
% 2026-08-29

# NAME

modbox-pstree - Display a tree of processes

# SYNOPSIS

**modbox pstree** [_OPTION_]... [_PID_]...

# DESCRIPTION

Display a tree of processes. By default the full process hierarchy rooted at
init is shown as an indented ASCII tree of process names. One or more PID
arguments restrict the display to the subtree(s) rooted at those processes.

# OPTIONS

`-a`, `--args`
:   Show command line arguments after each process name.

`-h`, `--help`
:   Display help and exit.

`-p`, `--show-pids`
:   Show PIDs in parentheses after each process name.

`-s`, `--show-parents`
:   Show the parents (ancestor chain up to init) of the selected process above
    its subtree.

`-u`, `--uid-info`
:   Show uid transitions: when a process's effective user differs from its
    parent's, the owning user name is annotated inline.

`-V`, `--version`
:   Output version information and exit.

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

**modbox-ps**(1), **modbox**(1)
