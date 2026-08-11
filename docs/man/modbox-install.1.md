% MODBOX-INSTALL(1) modbox | User Commands
% modbox project
% 2026-08-11

# NAME

modbox-install - copy files and set attributes

# SYNOPSIS

**modbox install** [*OPTION*]... **\-T** SOURCE DEST
**modbox install** [*OPTION*]... SOURCE... DIRECTORY
**modbox install** [*OPTION*]... **\-t** DIRECTORY SOURCE...
**modbox install** [*OPTION*]... **\-d** DIRECTORY...

In the first three forms, copy SOURCE to DEST or multiple SOURCE(s) to
the existing DIRECTORY, while setting permission modes and ownership.
In the fourth form, create all components of the specified DIRECTORY(ies).

# DESCRIPTION

Install (copy) files from SOURCE to DEST with optional permission and
ownership settings. When copying, missing parent directories of DEST are
created automatically (unless **-d** is used, in which case only directories
are created and no files are copied).

# OPTIONS

**-d**, **--directory**
:   Create all components of the destination path(s) as directories.
    When this option is used, SOURCE arguments are ignored and only
    directories are created.

**-m**, **--mode=MODE**
:   Set the permission mode to MODE (as in chmod). If not specified,
    the default mode is `0755` for directories and `0644` for files.

**-o**, **--owner=OWNER**
:   Set the ownership to OWNER (super-user only).

**-g**, **--group=GROUP**
:   Set the group ownership to GROUP (super-user only).

**-s**, **--strip**
:   Strip symbol tables and relocation information from copied binaries.

**--strip-program=PROGRAM**
:   Use PROGRAM instead of the default strip program.

**-C**, **--compare**
:   Compare each source with the destination before copying; skip files
    that are identical.

**-p**, **--preserve-timestamps**
:   Preserve access and modification times from the source file.

**-v**, **--verbose**
:   Print the name of each file as it is processed.

**-b**, **--backup**
:   Make a backup of each existing destination file (append **~**).

**-S**, **--suffix=SUFFIX**
:   Override the usual backup suffix (**~**) when using **-b**.

**-t**, **--target-directory=DIRECTORY**
:   Copy all SOURCE arguments into DIRECTORY. Equivalent to placing
    DIRECTORY after all SOURCE arguments.

**-T**, **--no-target-directory**
:   Treat DEST as a normal file, not a directory (used with a single
    SOURCE argument to override the default directory interpretation).

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Install a file with default permissions
modbox install myprog /usr/local/bin/

# Install with specific mode
modbox install -m 755 script.sh /usr/local/bin/

# Create directories only
modbox install -d /var/log/myapp /var/lib/myapp

# Copy and strip a binary
modbox install -s mylib.so /usr/local/lib/

# Compare before copying (idempotent install)
modbox install -C myconfig.conf /etc/myapp/

# Install multiple files into a directory
modbox install -v file1 file2 file3 /opt/app/bin/

# Backup existing files
modbox install -b -S .bak important.cfg /etc/myapp/
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- **-o** and **-g** require super-user privileges on most systems.
- The default backup suffix is **~** unless overridden by **-S**.
- When **-d** is used, the mode default is `0755` for created directories.
- **-T** is useful when DEST looks like a directory but should be treated
  as a regular file destination.
- **-C** makes the command idempotent: running it twice produces no change
  if the files are identical.

# SEE ALSO

**modbox-cp**(1), **modbox-chmod**(1), **modbox-chown**(1), **modbox**(1)
