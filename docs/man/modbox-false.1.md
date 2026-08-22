% MODBOX-FALSE(1) modbox | User Commands
% modbox project
% 2026-08-02

# NAME

modbox-false - return false

# SYNOPSIS

**modbox false** [*OPTION*]...
**modbox false** [*ARG*]...

# DESCRIPTION

Do nothing, unsuccessfully. Always exits with status 1.

# OPTIONS

**--help**
:   Display help and exit.

**--version**
:   Output version information and exit.

# EXAMPLES

```bash
modbox false
if modbox false; then echo yes; else echo no; fi
```

# EXIT STATUS

Always exits with status 1, except with `--help` or `--version`.

# SEE ALSO

**modbox-true**(1), **modbox**(1)
