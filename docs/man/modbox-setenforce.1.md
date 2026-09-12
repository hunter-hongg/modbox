% MODBOX-SETENFORCE(1) modbox | User Commands
% modbox project
% 2026-09-11

# NAME

modbox-setenforce - set the SELinux enforcement mode

# SYNOPSIS

**modbox setenforce** [ *Enforcing* | *Permissive* | *1* | *0* ]

**modbox setenforce** [*OPTION*]

# DESCRIPTION

Set the SELinux enforcement mode. The mode is selected by a single argument:

- `Enforcing` or `1` - enable enforcing mode
- `Permissive` or `0` - enable permissive mode

This command changes the global enforcement state and therefore typically
requires root privileges.

# OPTIONS

**--help**
:   Display help and exit.

**--version**
:   Output version information and exit.

# EXIT STATUS

`0`
:   The enforcement mode was set successfully.

non-zero
:   An invalid argument was given, or the mode could not be set.

# SEE ALSO

**modbox-getenforce**(1), **modbox-setenforce**(1), **modbox**(1)
