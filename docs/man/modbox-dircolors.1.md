% MODBOX-DIRCOLORS(1) modbox | User Commands
% modbox project
% 2026-08-21

# NAME

modbox-dircolors - output commands to set the LS_COLORS environment variable

# SYNOPSIS

**modbox dircolors** [*OPTION*]... [*FILE*]

# DESCRIPTION

Output commands for the shell to set the **LS_COLORS** environment variable,
which controls color formatting in **modbox ls** and other directory listing
tools.

With no *FILE*, or when *FILE* is `-`, the database is read from standard
input. The database is a list of keyword-value pairs, one per line, where
each line maps a file name pattern or object type to an ANSI color code.

modbox ships with a built-in default database covering common file types,
extensions, and permissions. Use **-p** to print this default database.

# OPTIONS

**-b**, **--sh**, **--bourne-shell**
:   Output Bourne shell (sh/bash) compatible commands.
    Produces a `LS_COLORS='...'` assignment followed by an `export` statement.

**-c**, **--csh**, **--c-shell**
:   Output C shell (csh/tcsh) compatible commands.
    Produces a `setenv LS_COLORS '...'` statement.

**-p**, **--print-database**
:   Print the default LS_COLORS database to stdout without any shell
    wrapping. The database can be redirected to a file and used as a
    starting point for customization.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# LS_COLORS FORMAT

The LS_COLORS value is a colon-separated list of `KEY=VALUE` pairs.

**KEY**
:   A keyword such as `DIR`, `EXEC`, `LNK`, `ORPHAN`, or a file extension
    such as `.tar`, `.jpg`.

**VALUE**
:   A semicolon-separated list of numeric ANSI escape codes, e.g. `01;36`
    for bold cyan.

Common keywords:

`DIR`
:   Directories

`EXEC`
:   Executable files

`LNK`
:   Symbolic links

`ORPHAN`
:   Broken symbolic links

`MISSING`
:   Referenced files that do not exist

`REGULAR`
:   Normal files

`BLK`
:   Block device files

`CHR`
:   Character device files

`FIFO`
:   Named pipes

`SOCK`
:   Unix domain sockets

`STICKY_OTHER_WRITABLE`
:   Other-writable directories with sticky bit

`OTHER_WRITABLE`
:   Other-writable directories

`CAPABILITY`
:   Files with POSIX capabilities

# EXAMPLES

```bash
# Generate shell commands for ls colors
eval "$(modbox dircolors -b)"

# Print the default database for inspection
modbox dircolors -p

# Save the default database to a custom file
modbox dircolors -p > ~/.dircolors

# Customize and apply
modbox dircolors ~/.dircolors -b | source -

# Generate csh commands
eval "$(modbox dircolors -c)"
```

# EXIT STATUS

`0`
:   Success.

non-zero
:   An error occurred.

# NOTES

- The default database includes entries for common archive extensions
  (`.tar`, `.gz`, `.xz`, `.zip`, etc.), media files, and special file types.
- **LS_COLORS** is read by **modbox ls** at startup; changes take effect
  immediately in new shell sessions.
- Numeric codes follow the standard ANSI SGR format: `00` (normal),
  `01` (bold), `30`–`37` (foreground colors), `40`–`47` (background colors).

## Differences from GNU dircolors

Not implemented: `-S` / `--shell` to force a specific shell,
environment variable parsing from `/etc/DIR_COLORS` or `~/.dircolors`
when no FILE is given (stdin is used instead). The built-in database is
a subset of the GNU default.

# SEE ALSO

**modbox-ls**(1), **dircolors**(1), **environment**(7)
