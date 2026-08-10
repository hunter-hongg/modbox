% MODBOX-BASENAME(1) modbox | User Commands
% modbox project
% 2026-08-10

# NAME

modbox-basename - print the base name of a file path

# SYNOPSIS

**modbox basename** [**-a**, **--multiple**] [**-s**, **--suffix**=SUFFIX] [**-z**, **--zero**] NAME...

**modbox basename** [**-a**, **--multiple**] [**-s**, **--suffix**=SUFFIX] [**-z**, **--zero**] NAME SUFFIX

# DESCRIPTION

Print the base name of each NAME (the final path component with any
leading directory components removed). When multiple names are given
with **-a**/**--multiple**, each is processed independently.

If a trailing SUFFIX is also given, it is removed from the result.
The **--suffix** option implies **-a**/**--multiple**; when both **-a**
and **-s** are used together, each NAME is stripped of the trailing
SUFFIX.

# OPTIONS

**-a**, **--multiple**
:   Support multiple arguments and treat each as a separate NAME.
    Without this option, only the last argument is processed as NAME
    and any preceding arguments are interpreted as SUFFIX.

**-s**, **--suffix**=SUFFIX
:   Remove a trailing SUFFIX from each name. This implies **-a**/**--multiple**.

**-z**, **--zero**
:   End each output line with a NUL character (`\0`) instead of a
    newline. Useful for piping into **modbox-xargs** with **--null**.

**--help**
:   Display help and exit.

**--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Print just the file name from a path
modbox basename /usr/local/bin/modbox
# → modbox

# Remove a trailing suffix
modbox basename /path/to/file.txt .txt
# → file

# Process multiple names and strip suffix
modbox basename -a -s .txt /path/to/file1.txt /path/to/file2.txt
# → file1
# → file2

# NUL-terminated output for safe xargs piping
modbox basename -z /foo/bar.txt .txt | modbox xargs -0 modbox-echo
```

# EXIT STATUS

`0`
:   Success.

`1`
:   An error occurred (e.g. invalid option).

# NOTES

- Without **-a**, the first argument is interpreted as NAME and the
  second (if present) as SUFFIX. This differs from GNU coreutils
  where only the last argument is treated as NAME.
- The **-s** option implies **-a** when given alone; passing both
  **-a** and **-s** explicitly has the same effect.
- When NAME contains no `/`, the name is printed unchanged (minus
  any trailing SUFFIX).

# SEE ALSO

**modbox-dirname**(1), **modbox-xargs**(1), **modbox**(1)
