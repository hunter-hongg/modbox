% MODBOX-MKTEMP(1) modbox | User Commands
% modbox project
% 2026-08-11

# NAME

modbox-mktemp - create a temporary file or directory

# SYNOPSIS

**modbox mktemp** [*OPTION*]... [*TEMPLATE*]

# DESCRIPTION

Create a unique temporary file and print its name. The TEMPLATE must
contain at least three consecutive **X** characters (e.g., `tempXXXXXX`).
If TEMPLATE is omitted, the default is `tmp.XXXXXXXXXX` in the current
directory.

# OPTIONS

**-t**, **--tempdir=PREFIX**
:   Interpret TEMPLATE as a directory name prefix; the temporary file is
    created inside that directory. The TEMPLATE must still contain `X`
    characters for the unique suffix.

**-h**, **--help**
:   Display help and exit.

# TEMPLATE SYNTAX

TEMPLATE is a path ending in one or more `X` characters. The trailing
`X`s are replaced with a unique string to produce a file name that does
not already exist. Examples:

`/tmp/myapp.XXXXXX`
:   Creates a file in `/tmp` with a 6-character random suffix.

`modbox-XXXXXX`
:   Creates a file in the current directory prefixed with `modbox-`.

# EXAMPLES

```bash
# Create a temporary file in the current directory
modbox mktemp

# Create with a specific template
modbox mktemp /tmp/myapp.XXXXXXX

# Create inside a specific directory
modbox mktemp -t /tmp/myapp.XXXXX

# Use the result in a script
TMPFILE=$(modbox mktemp)
trap "rm -f $TMPFILE" EXIT
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- Without **-t**, mktemp creates the temporary file in the **current
  directory**, not `/tmp`. This is a deviation from GNU coreutils, which
  defaults to `/tmp` when the template contains a directory component.
- For security-sensitive scripts, always use an explicit directory prefix
  and a template with enough `X` characters to avoid predictable names.
- The created file is owned by the current user with mode `0600`.

# SEE ALSO

**modbox-mkdir**(1), **modbox**(1)
