% MODBOX-MAN(1) modbox | User Commands
% modbox project
% 2026-09-11

# NAME

modbox-man - display manual pages for modbox commands

# SYNOPSIS

**modbox man** [*OPTION*]... [*PAGE*]

# DESCRIPTION

Display the manual page for a modbox command. With no arguments, **man**
prints its usage summary.

The page source files live in `docs/man/modbox-*.<n>.md`. Pages are rendered
with pandoc and, when available, displayed through a pager; otherwise the
rendered text is written to standard output.

Only section 1 pages are currently implemented; **man** ignores section
numbers and always looks for section 1.

# OPTIONS

**-k**, **--apropos** *KEYWORD*
:   Search the manual pages for *KEYWORD* and list matching pages with their
    one-line descriptions.

**-f**, **--whatis** *PAGE*
:   Display the one-line description of *PAGE*.

**-a**, **--all**
:   Show all matching pages.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# EXIT STATUS

`0`
:   The requested page was found and displayed.

non-zero
:   The page was not found, or a usage error occurred.

# SEE ALSO

**modbox-help**(1), **modbox**(1)
