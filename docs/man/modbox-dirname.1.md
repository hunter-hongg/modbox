% MODBOX-DIRNAME(1) modbox | User Commands
% modbox project
% 2026-08-10

# NAME

modbox-dirname - print the directory name of a file path

# SYNOPSIS

**modbox dirname** [**-z**, **--zero**] NAME...

# DESCRIPTION

Print NAME with its trailing /-component removed. If NAME contains no
slash, print `.` (the current directory). The result is always the
logical parent directory of the given path.

Multiple names may be given; each is processed independently.

# OPTIONS

**-z**, **--zero**
:   End each output line with a NUL character (`\0`) instead of a
    newline. Useful for piping into **modbox-xargs** with **--null**.

**--help**
:   Display help and exit.

**--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Print the directory part of a path
modbox dirname /usr/local/bin/modbox
# → /usr/local/bin

# Name with no slash returns '.'
modbox dirname justname
# → .

# NUL-terminated for safe piping
modbox dirname -z /foo/bar /baz/qux | modbox xargs -0 modbox-echo
```

# EXIT STATUS

`0`
:   Success.

`1`
:   An error occurred (e.g. invalid option).

# NOTES

- When NAME contains no `/`, the result is always `.` — this matches
  GNU coreutils behavior.
- The **-a**/**--multiple** option is not available; multiple names are
  always processed by supplying them as separate arguments.
- This command is the inverse of **modbox-basename**: for a path
  containing at least one `/`, `dirname` yields the parent directory
  and `basename` yields the final component.

# SEE ALSO

**modbox-basename**(1), **modbox-xargs**(1), **modbox**(1)
