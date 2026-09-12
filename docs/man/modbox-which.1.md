% MODBOX-WHICH(1) modbox | User Commands
% modbox project
% 2026-09-12

# NAME

modbox-which - locate a command

# SYNOPSIS

**modbox which** [*OPTION*]... [**--**] *COMMAND*...

# DESCRIPTION

For each *COMMAND* given, **modbox which** searches the directories listed in
the **PATH** environment variable (in order) for an executable regular file
with that name and prints its full path. When **-a/--all** is given, every
match is printed; otherwise the search stops at the first match for each name.

A *COMMAND* that contains a slash is not searched in **PATH**; it is used
directly and printed when it is executable.

Each name that cannot be resolved produces a message on standard error and
affects the exit status:

```
which: no NAME in (PATH...)
```

Resolved names are always printed on standard output, even when other names
on the same command line fail.

The **PATH** variable is read at invocation time, defaulting to
`/usr/bin:/bin` when it is unset or empty. Empty **PATH** segments are
ignored.

# OPTIONS

**-a**, **--all**
:   Print all matching paths in **PATH** order, not just the first match for
    each name.

**--skip-dot**
:   Skip **PATH** entries that begin with a dot.

**--skip-tilde**
:   Skip **PATH** entries that begin with a tilde.

**--show-dot**
:   Do not expand a `.` **PATH** entry to the current working directory in
    the printed output. The entry is still searched as the current directory.

**--show-tilde**
:   Render a **PATH** entry that is (or lies under) **HOME** using a leading
    `~` in the printed output, when the user is not root.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# EXIT STATUS

`0`
:   Every requested name resolved. When no *COMMAND* is given, usage is
    printed and the exit status is `0`.

`1`
:   Some, but not all, requested names resolved.

`2`
:   No requested name resolved, or an option was invalid.

# EXAMPLES

```bash
# Print the first match for bash
modbox which bash

# Print every match for python3, in PATH order
modbox which -a python3

# Ignore dot-directories in PATH
modbox which --skip-dot gcc

# Resolve a path-like argument directly
modbox which /usr/bin/ls
```

# NOTES

- The shell introspection options of GNU **which**
  (`-i/--read-alias`, `--read-functions`, `--tty-only`, and the
  `--skip-alias`/`--skip-functions` variants) are not supported.
- An unknown option prints `which: invalid option -- 'X'` to standard error
  and exits with status 2.

# SEE ALSO

**modbox-whereis**(1), **modbox-command**(1), **modbox**(1)
