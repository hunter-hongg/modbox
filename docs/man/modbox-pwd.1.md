% MODBOX-PWD(1) modbox | User Commands
% modbox project
% 2026-08-10

# NAME

modbox-pwd - print the full filename of the current working directory

# SYNOPSIS

**modbox pwd** [**-L**, **--logical** | **-P**, **--physical**]

# DESCRIPTION

Print the full absolute path of the current working directory.

With no option, the default behavior is **-P** (physical): all
symlink components in the path are resolved to their real targets.

The **-L** option uses the value of the environment variable
**PWD** as-is, even if it contains symlink components that differ
from the actual path on disk.

# OPTIONS

**-L**, **--logical**
:   Use the value of the PWD environment variable, even if it
    contains symbolic links.

**-P**, **--physical**
:   Resolve all symbolic links in the path. This is the default
    when no option is given.

**--help**
:   Display help and exit.

**--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Print the physical (real) path — default behavior
modbox pwd
# → /home/user/projects/modbox

# Print the logical path from PWD
modbox pwd -L
# → /home/user/modbox  (if PWD was set via a symlink)

# Force physical path resolution
modbox pwd -P
# → /home/user/projects/modbox
```

# EXIT STATUS

`0`
:   Success.

`1`
:   An error occurred (e.g. invalid option or PWD is not set).

# NOTES

- The default behavior (no option) is equivalent to **-P**.
- GNU coreutils supports a **--print-symlink** flag as an alias for
  **-L**; this is not supported in modbox. Use **-L** instead.
- When running inside a shell, `pwd` may be a shell builtin with
  slightly different semantics. Always invoke the standalone
  **modbox pwd** for consistent behavior.

# SEE ALSO

**modbox-cd**(1), **modbox-realpath**(1), **modbox-find**(1), **modbox**(1)
