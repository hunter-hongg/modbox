% MODBOX-FINDMNT(1) modbox | User Commands
% modbox project
% 2026-08-31

# NAME

modbox-findmnt - Find a filesystem by device or mount point

# SYNOPSIS

**modbox findmnt** [*OPTIONS*] [*device* | *mountpoint*]

# DESCRIPTION

List mounted filesystems, or search for a specific one. By default **findmnt**
reads the kernel's mount table from `/proc/self/mountinfo`, falling back to
`/proc/mounts` when the former is unavailable, and prints the result as a
tree that mirrors the mount hierarchy.

With no positional argument every mount is listed. When a positional argument
is given, it is treated as a mount point if it begins with `/` (matching that
path and, by default, everything mounted beneath it), and as a source device
otherwise.

The command is read-only and requires no special privileges.

# OPTIONS

`-a`, `--all`
:   List all filesystems. This is the default behaviour.

`-c`, `--canonicalize`
:   Print only three space-separated fields per line: target, source and
    filesystem type. Intended for scripts.

`-J`, `--json`
:   Emit a JSON array with one object per mount.

`-l`, `--list`
:   Print a flat list instead of a tree, one mount per line.

`-n`, `--noheadings`
:   Suppress the column header row.

`-o`, `--output` *LIST*
:   Select the columns to print as a comma-separated list. The option may be
    repeated. See **COLUMNS** below. Defaults to
    `TARGET,SOURCE,FSTYPE,OPTIONS`.

`-P`, `--pairs`
:   Print `KEY="value"` pairs, one mount per line, using uppercase keys.
    Implies a flat listing.

`-r`, `--raw`
:   Print `key="value"` pairs with lowercase keys, one mount per line.
    Implies a flat listing.

`-R`, `--submounts`
:   When searching for a mount point, print the mounts *below* it instead of
    the mount itself.

`-S`, `--source` *device*
:   Restrict the output to mounts whose source matches *device*.

`-T`, `--target` *mountpoint*
:   Restrict the output to mounts at *mountpoint*.

`-t`, `--types` *LIST*
:   Restrict the output to the comma-separated list of filesystem types. The
    option may be repeated.

`-v`, `--invert`
:   Invert the `--types` filter, i.e. exclude the named types.

`-V`, `--version`
:   Output version information and exit.

`-h`, `--help`
:   Display help and exit.

# COLUMNS

Column names are matched case-insensitively and accept the usual single-letter
abbreviations.

- **TARGET** (**T**) — mount point.
- **SOURCE** (**S**) — source device or pseudo-device.
- **FSTYPE** (**F**) — filesystem type.
- **OPTIONS** (**O**) — combined per-mount and superblock options.
- **MAJ:MIN** — device major:minor numbers.
- **ROOT** (**/) — root of the filesystem (from the mount table).
- **ID** — mount ID.
- **PARENT** — ID of the parent mount.

# EXAMPLES

Show the whole mount tree:

```
modbox findmnt
```

Show only the mount at `/proc`:

```
modbox findmnt /proc
```

List every `tmpfs` mount as a flat list without a header:

```
modbox findmnt -l -n -t tmpfs
```

Show source and filesystem type for `/`, canonicalised:

```
modbox findmnt -c -o SOURCE,FSTYPE /
```

Emit the whole table as JSON:

```
modbox findmnt -J
```

# EXIT STATUS

`0` on success.

`1` when the query matches nothing, when the mount table cannot be read, or on
an argument-parsing error (unknown option, missing option argument).

`2` on a semantic usage error: an unknown `--output` column or the unsupported
`--fstab` option.

# NOTES

`/etc/fstab` is not consulted; the `--fstab`/`-F` option is not supported and
exits with status `2`.

# SEE ALSO

**modbox-mount**(1), **modbox-umount**(1), **modbox-df**(1), **modbox-lsblk**(1)
