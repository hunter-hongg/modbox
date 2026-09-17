% MODBOX-NAMEI(1) modbox | User Commands
% modbox project
% 2026-09-03

# NAME

modbox-namei - Follow a pathname until a terminal point is found

# SYNOPSIS

**modbox namei** [*OPTIONS*] *pathname*...

# DESCRIPTION

For every *pathname* **namei** prints a `f: pathname` header followed by one line
per component of the path, so the type and ownership of every element of the
chain can be checked at a glance. This makes it the fastest way to locate which
component of a path does not exist or cannot be traversed.

Each component line begins with a type character taken from the file mode:

| character | type           |
| --------- | -------------- |
| `d`       | directory      |
| `-`       | regular file   |
| `l`       | symbolic link  |
| `b`       | block device   |
| `c`       | character dev. |
| `s`       | socket         |
| `p`       | FIFO pipe      |
| `m`       | MS-DOS (fat)   |
| `n`       | NFS            |
| `u`       | unknown / not a special file |

Symbolic links are resolved recursively: the link is printed as
`name -> target` and the components of the target follow, indented by two
spaces per resolution level. A relative target is joined to the directory of
the link before it is resolved; an absolute target is shown from its own root.

# OPTIONS

`-x`, `--mountpoints`
:   Print mount point directories with a `D` instead of `d`. A directory is
    reported as a mount point when it lives on a different device from its
    parent, or when it is a root directory (the same inode as its parent).

`-m`, `--modes`
:   Print the full 10-character mode string (as `ls -l` renders it) instead of
    just the type character.

`-o`, `--owners`
:   Print the owner and group name of every component. An id without a name
    renders as its decimal number. The two columns are padded to the widest
    name seen across the whole invocation, so several paths line up
    column-wise.

`-l`, `--long`
:   Use a long listing format, equivalent to `-m -o -v`.

`-n`, `--nosymlinks`
:   Print symbolic links without resolving them.

`-v`, `--vertical`
:   Vertically align the modes and owners: the indentation is printed after
    the mode string rather than before it.

`-h`, `--help`
:   Display the usage message and exit.

`-V`, `--version`
:   Print version information and exit.

# EXAMPLES

Show the short listing of a path:

```sh
$ modbox namei /etc/resolv.conf
f: /etc/resolv.conf
 d /
 d etc
 - resolv.conf
```

Same path in long format, with modes, owner and group:

```sh
$ modbox namei -l /etc
f: /etc
dr-xr-xr-x root root /
drwxr-xr-x root root etc
```

Follow a symlink chain, and inspect it without resolving:

```sh
$ modbox namei /tmp/link.txt
f: /tmp/link.txt
 d /
 d tmp
 l link.txt -> real.txt
    - real.txt
```

`modbox namei -n /tmp/link.txt` stops at the link and prints only
`l link.txt -> real.txt`.

Locate the mount points along a path:

```sh
$ modbox namei -x /etc
f: /etc
 D /
 d etc
```

Diagnose an inaccessible path:

```sh
$ modbox namei /opt/private/deep/file
f: /opt/private/deep/file
 d /
 d opt
 - private - Permission denied
```

# EXIT STATUS

`0`
:   Every component of every path was resolved and printed.

`1`
:   A path could not be stat'ed, a component does not exist or cannot be
    read, or the number of traversed symlinks exceeded 256.

`2`
:   Usage error (no path given, or an unrecognized option).

# NOTES

Upstream **namei** is part of util-linux. Two deviations from the reference
behaviour are intentional:

- Usage errors exit with status 2 instead of util-linux' `EX_USAGE` (1).
- The error prefix in messages is the command name rather than the invoking
  path (`namei: unrecognized option ...`), and `-q`-style short options are
  reported with the repository-wide argtable3 wording
  (`unexpected argument`) rather than `invalid option`.

`-Z`, `--context` (SELinux context per component) is not implemented and is
rejected as an unrecognized option.

# SEE ALSO

**stat**(1), **ls**(1), **readlink**(1), **lsblk**(8)
