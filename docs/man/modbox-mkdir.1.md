% MODBOX-MKDIR(1) modbox | User Commands
% modbox project
% 2026-08-07

# NAME

modbox-mkdir - create directories

# SYNOPSIS

**modbox mkdir** [*OPTION*]... *DIRECTORY*...

# DESCRIPTION

Create the DIRECTORY(ies), if they do not already exist.
Multiple directories may be specified; each is created in order.

# OPTIONS

**-m**, **--mode=MODE**
:   Set the file permission mode (as in **chmod**), rather than the
    default `0777` modified by the umask.
    MODE is interpreted as an octal number (e.g., `0755`, `755`) or
    a symbolic mode is **not** supported.
    Valid range is `0` to `07777`.

**-p**, **--parents**
:   No error if existing directory; create parent directories as
    needed.
    For example, `mkdir -p a/b/c` creates `a`, `a/b`, and `a/b/c`
    if they do not exist.
    If an intermediate directory already exists, no error is raised.
    If an intermediate directory is non-empty, it is silently skipped
    (matching GNU coreutils behaviour).

**-v**, **--verbose**
:   Print a message for each created directory, e.g.
    `mkdir: created directory 'a/b/c'`.

**-h**, **--help**
:   Display help and exit.

# MODE FORMAT

The **--mode** (or **-m**) argument is parsed as an octal integer.
Common values:

`0755`
:   Owner can read/write/execute; group and others can read/execute.

`0700`
:   Owner can read/write/execute; no access for group or others.

`0644`
:   Owner can read/write; group and others can read only.

The mode is applied after the umask is cleared (i.e., the mode
specified is the actual mode, not masked by umask).

# NOTES

- Without **-p**, mkdir fails if any parent directory does not exist.
- Without **-p**, mkdir fails if the target directory already exists.
- With **-p**, creating an already-existing directory is not an error.
- With **-p**, the command returns success even if some directories
  in the path could not be created (it reports errors for each but
  continues with the rest).
- The default mode is `0777`, which is then modified by the process
  umask.

## Differences from GNU mkdir

Not implemented: `--no-preserve-root` (modbox does not treat `/`
specially by default), `--ignore-existing`, `--suffix=SUFFIX`
(for backup), recursive `--parents` with `-m` on non-existent parents
is supported but intermediate permission handling follows the simple
octal interpretation.

# EXAMPLES

```bash
# Create a single directory
modbox mkdir newdir

# Create a directory with specific permissions
modbox mkdir -m 0700 private

# Create nested directories
modbox mkdir -p a/b/c

# Create multiple directories at once
modbox mkdir dir1 dir2 dir3

# Verbose output showing each creation
modbox mkdir -pv a/b/c

# Create with mode, ignoring existing parents
modbox mkdir -m 0755 -p /var/log/app
```

# EXIT STATUS

`0`
:   All directories were created successfully, or already existed
    (with **-p**).

non-zero
:   An error occurred (e.g., permission denied, invalid mode).

# SEE ALSO

**modbox-rmdir**(1), **modbox-rm**(1), **modbox**(1)
