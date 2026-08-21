% MODBOX-DIR(1) modbox | User Commands
% modbox project
% 2026-08-21

# NAME

modbox-dir - list directory contents in vertical format

# SYNOPSIS

**modbox dir** [*OPTION*]... [*FILE*]...

# DESCRIPTION

List the contents of each given *FILE* (or the current directory if no
files are given) in a vertical column format.

**dir** is functionally equivalent to **modbox ls -C**. It prepends the
**-C** flag internally and delegates to the ls command.

# OPTIONS

All options are passed through to **modbox ls**. See **modbox-ls**(1) for
the complete list of supported options.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# List current directory vertically
modbox dir

# List a specific directory
modbox dir /usr/local/bin

# List with detailed information
modbox dir -l /etc

# List hidden files as well
modbox dir -a
```

# EXIT STATUS

`0`
:   Success.

non-zero
:   An error occurred.

# NOTES

- **dir** exists primarily as a compatibility alias for systems where
  `ls -C` is not the default behavior.
- All output formatting is controlled by the underlying **ls** command.

# SEE ALSO

**modbox-ls**(1), **ls**(1)
