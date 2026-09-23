# modbox-tree(1)

## Name

**tree** — list contents of directories in a tree-like format

## Synopsis

```
modbox tree [OPTION]... [PATH]...
```

## Description

**tree** lists the contents of each directory recursively, one entry per line,
indented with box-drawing connectors so the shape of the hierarchy is visible.
A summary of how many directories and files were listed closes the output
unless `--noreport` is given.

With no `PATH`, the working directory is listed. Each `PATH` is listed in the
order given, and its name is printed as it was given.

Hidden entries (names starting with `.`) are omitted unless `-a` is given.
Names containing control characters or invalid bytes are escaped as in an
`ls` listing (`\012` for a newline, `\\` for a backslash).

Entries are sorted by name using the collation of the current locale, with
`--dirsfirst` and `--filesfirst` grouping directories and files ahead of each
other. A symbolic link that points to a directory counts as a directory for
sorting and for the summary, and is only descended into with `-l`.

## Options

### Listing

| Option | Description |
|---|---|
| `-a`, `--all` | List hidden files too. |
| `-d` | List directories only. |
| `-l` | Follow symbolic links that point to directories. |
| `-f` | Print each entry's full path from the listing root instead of its base name. |
| `-x` | Stay on the filesystem the listing starts on; do not descend into a mount point. |
| `-L LEVEL` | Descend at most `LEVEL` directories deep (must be greater than 0). |
| `-i` | Print no indentation, one entry per line. |
| `--noreport` | Suppress the trailing summary line. |

### Filtering

| Option | Description |
|---|---|
| `-I PATTERN` | Do not list entries whose name matches `PATTERN`. |
| `-P PATTERN` | List files that match `PATTERN` (directories are kept so their matching contents stay reachable). |

Both patterns are shell-style globs — not regular expressions — matched
against the whole name. Several globs are joined with `|`, as in
`-I '.*\.o|*.c'`. An empty `PATTERN` excludes nothing.

### Metadata

| Option | Description |
|---|---|
| `-s` | Print the size in bytes of each entry; a directory shows its own `st_size`. |
| `--du` | Print each directory's size as the sum of its own size and its contents. |
| `-h` | Print sizes with a `K`/`M`/`G`… suffix, right-aligned in the same field (implies `-s`). |
| `-D` | Print the last modification date, or the status change date with `-c`. |
| `--inodes` | Print the inode number of each entry. |
| `--selinux` | Print the SELinux security context of each entry. |
| `-F` | Append `/` to directories, `*` to executables, `=` to sockets and `\|` to FIFOs, as `ls -F` does. |
| `--charset=ascii` | Draw the tree with `\|--`, `` `-- `` and `\|` instead of Unicode box characters. |

`-s`, `--du`, `-D` and `--inodes` share one bracket before the name, in that
order, with each field right-aligned.

The date is rendered as `%b %e %H:%M`; entries dated more than six months ago,
or in the future, show `%b %e  %Y` instead.

### Ordering

| Option | Description |
|---|---|
| `--sort=TYPE` | Sort by `name`, `version`, `size`, `mtime`, `ctime` or `none`. |
| `-v` | Same as `--sort=version`: digit runs compare numerically, so `v2` precedes `v10`. |
| `-t` | Same as `--sort=mtime`. |
| `-c` | Same as `--sort=ctime`, and `-D` then reports the status change time. |
| `-U` | Same as `--sort=none`: keep the order the directory was read in. |
| `-r` | Reverse the sort order. |
| `--dirsfirst` | List directories before files. |
| `--filesfirst` | List files before directories. |

`size` sorts largest first, `mtime` and `ctime` oldest first, and every mode
breaks ties by name.

## Output notes

A directory that cannot be opened is listed with `  [error opening dir]`, and a
non-directory `PATH` is reported the same way. With `-l`, a symbolic link whose
target directory has already been listed is annotated
`  [recursive, not followed]` instead of being expanded again; each directory is
expanded once, so which link wins depends on the order entries are visited —
display order normally, directory-read order under `--du`, where the size of a
directory must be settled while it is still being read.

## Exit Status

- **0** — success
- **1** — usage error (bad option, or `-L` with a level below 1)
- **2** — at least one path could not be listed

## Examples

```sh
# List a directory tree, including hidden files
modbox tree -a /etc

# Directory-only view, two levels deep
modbox tree -d -L 2 /home

# Space taken by each directory, in human-readable form
modbox tree --du -h /var

# Stay on one filesystem, newest first
modbox tree -x -t /home

# Only the sources, without build output
modbox tree -P '*.[ch]' -I 'build|*.o' src
```

## Differences from GNU tree

- The indentation after a vertical connector is a plain space; GNU tree emits a
  non-breaking space there.
- `--selinux` prints nothing on a system without SELinux, where GNU tree prints
  an empty context bracket.
- HTML and XML output are not implemented: `-H`, `-J`, `-X`, `-o`, `-T`,
  `--hintro`, `--houtro`, `--nolinks`, `--scheme`, `--authority`,
  `--hyperlink`, `--opt-toggle`, `--compress`, `--condense`.
- Other GNU tree options that are rejected here as unknown: `-A`, `-C`, `-N`,
  `-Q`, `-S`, `-g`, `-n`, `-p`, `-q`, `-u`, `--acl`, `--device`, `--fflinks`,
  `--filelimit`, `--fromfile`, `--fromtabfile`, `--gitfile`, `--fromtabfile`, `--gitfile`,
  `--gitignore`, `--ignore-case`, `--info`, `--infofile`, `--matchdirs`,
  `--metafirst`, `--prune`, `--si`, `--timefmt`. GNU tree spells `--all` as
  `-a` only, and `-Z` for the security context, which is `--selinux` here.

## See Also

**du**(1), **find**(1), **ls**(1), **modbox-namei**(1)
