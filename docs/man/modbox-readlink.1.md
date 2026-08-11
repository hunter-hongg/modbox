% MODBOX-READLINK(1) modbox | User Commands
% modbox project
% 2026-08-11

# NAME

modbox-readlink - print resolved symbolic link value

# SYNOPSIS

**modbox readlink** [*OPTION*]... *FILE*

# DESCRIPTION

Write the contents of SYMBOLIC LINK to standard output.
By default, the raw target of the symlink is printed without resolving it.
Use **-f** to resolve all symlinks and print the canonical path.

# OPTIONS

**-f**, **--canonicalize**
:   Resolve all symbolic links in the path and print the canonical absolute
    path. Returns an error status if the file does not exist.

**-q**, **--no-error**
:   Suppress error messages for nonexistent or invalid inputs.

**-s**, **--strip**
:   Strip any trailing slashes from the result.

**-n**, **--no-dereference**
:   Do not append a trailing newline to the output.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Print raw symlink target
modbox readlink mylink

# Resolve to canonical path
modbox readlink -f /path/to/symlink

# Suppress error on missing file
modbox readlink -q /nonexistent

# No trailing newline (useful in scripts)
modbox readlink -n /path/to/link
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- Without any flags, **readlink** prints the raw symlink target as stored
  in the inode, without resolving parent directories or symlinks.
- **-f** requires the file to exist; it resolves every component of the path.
- **-n** is commonly used in shell pipelines to avoid a trailing newline
  when the result is assigned to a variable.

# SEE ALSO

**modbox-realpath**(1), **modbox-ls**(1), **modbox**(1)
