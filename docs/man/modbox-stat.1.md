% MODBOX-STAT(1) modbox | User Commands
% modbox project
% 2026-08-07

# NAME

modbox-stat - display file or filesystem status

# SYNOPSIS

**modbox stat** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Display file or filesystem status information.
With no FILE, or when FILE is `-`, the behaviour depends on the mode.
Multiple files may be given; each is processed independently.

# OPTIONS

**-L**, **--dereference**
:   Follow symbolic links. Without this option, information about the
    link itself is reported.

**-f**, **--file-system**
:   Display filesystem status instead of file status.
    When this flag is used, each FILE argument is interpreted as a
    path on the filesystem to inspect.

**-t**, **--terse**
:   Print the information in terse single-line form instead of the
    default verbose multi-line format.

**-c**, **--format=FORMAT**
:   Use the specified FORMAT instead of the default verbose or terse
    format.  The default includes a trailing newline.

**--printf=FORMAT**
:   Like **--format**, but interpret backslash escape sequences and do
    **not** output a trailing newline.
    Supported escapes: `\a`, `\b`, `\f`, `\n`, `\r`, `\t`, `\v`,
    `\\`, `\"`, octal (`\NNN`), and hex (`\xNN`).

**--json**
:   Output the results in JSON format.
    This is a modbox extension.
    **--json** is incompatible with **--format** and **--printf**.

**-h**, **--help**
:   Display help and exit.

# FORMAT SPECIFIERS

When **--format** or **--printf** is used, the FORMAT string may contain
the following conversion specifiers.  Width and precision modifiers are
supported (e.g. `%10s`, `%-20.10s`).

## File mode specifiers (without **--file-system**)

`%n`
:   File name.

`%N`
:   Quoted file name with symlink target (e.g. `'foo' -> 'bar'`).

`%F`
:   File type (e.g. "regular file", "directory", "symbolic link").

`%s`
:   Total size in bytes.

`%b`
:   Blocks allocated (in units of 512 bytes).

`%B`
:   Size of each block reported by `%b` (always 512).

`%a`
:   Access rights in octal (e.g. `755`).

`%A`
:   Access rights in human-readable form (e.g. `rwxr-xr-x`).
    Includes special bits: SUID (`s`/`S`), SGID (`s`/`S`), sticky (`t`/`T`).

`%i`
:   Inode number.

`%h`
:   Number of hard links.

`%U`
:   User name of owner.

`%u`
:   Numeric user ID of owner.

`%G`
:   Group name of owner.

`%g`
:   Numeric group ID of owner.

`%w`
:   Time of birth, human-readable (locale-dependent).

`%W`
:   Time of birth, seconds since Epoch.

`%x`
:   Time of last access, human-readable.

`%X`
:   Time of last access, seconds since Epoch.

`%y`
:   Time of last modification, human-readable.

`%Y`
:   Time of last modification, seconds since Epoch.

`%z`
:   Time of last status change, human-readable.

`%Z`
:   Time of last status change, seconds since Epoch.

`%d`
:   Device number in decimal.

`%D`
:   Device number in hex.

`%f`
:   Raw mode in hex.

`%t`
:   Major device type in hex.

`%T`
:   Minor device type in hex.

`%o`
:   Optimal I/O transfer size in bytes.

`%m`
:   Mount point of the file.

## Filesystem mode specifiers (with **--file-system**)

`%n`
:   File name.

`%i`
:   Filesystem ID in hex.

`%l`
:   Maximum length of filenames.

`%t`
:   Filesystem type in hex.

`%T`
:   Filesystem type name (e.g. "ext4", "tmpfs").

`%s`
:   Block size for transfers.

`%S`
:   Fundamental filesystem block size.

`%b`
:   Total data blocks.

`%f`
:   Free blocks available to non-privileged users.

`%a`
:   Available blocks.

`%c`
:   Total inodes.

`%d`
:   Free inodes.

# NOTES

- When no format is given, the default verbose multi-line format is used
  for file mode, and the default filesystem format is used for filesystem
  mode.
- **--printf** is the preferred option for scripts because it does not
  append a newline and supports escape sequences.
- Birth time (`%w`, `%W`) is only available on Linux with `statx(2)`;
  otherwise it prints `-` or `0`.
- The `--json` output groups each file's information as a JSON object
  in a top-level array.

## Differences from GNU stat

Not implemented: `--no-newline` (use `--printf` instead),
`--dereference-command-line`, `--dereference-command-line-symlink-to-dir`,
custom date format with `--time`, `--format` with multiple files
(separate outputs per file).

# EXAMPLES

```bash
# Default verbose output
modbox stat myfile.txt

# Terse single-line output
modbox stat -t myfile.txt

# Custom format
modbox stat -c "%n: %s bytes, mode %a" myfile.txt

# printf mode with escape interpretation
modbox stat --printf="%n is %s bytes\n" myfile.txt

# Follow symlinks
modbox stat -L symlink

# Filesystem status
modbox stat -f /home

# JSON output
modbox stat --json myfile.txt

# Show human-readable permissions and owner
modbox stat -c "%A %U %n" /etc/passwd
```

# EXIT STATUS

`0`
:   Successful execution.

non-zero
:   An error occurred (e.g., file not found, invalid format).

# SEE ALSO

**modbox-ls**(1), **modbox-find**(1), **modbox**(1)
