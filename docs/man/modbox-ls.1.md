% MODBOX-LS(1) modbox | User Commands
% modbox project
% 2026-08-02

# NAME

modbox-ls - list directory contents

# SYNOPSIS

**modbox ls** [*OPTION*]... [*DIR*]...

# DESCRIPTION

List directory contents. With no DIR, list the current directory.

# OPTIONS

## Display options

`-a`, `--all`
:   Do not ignore entries starting with `.`.

`-A`, `--almost-all`
:   Do not list implied `.` and `..`.

`-l`, `--long`
:   Use the long listing format.

`--author`
:   With `-l`, also print the author of each file.

`-b`, `--escape`
:   Print C-style escapes (`\ooo`) for non-graphic characters.

`-B`, `--ignore-backups`
:   Do not list entries ending with `~`.

`-d`, `--directory`
:   List directories themselves, not their contents.

`-C`
:   List entries by columns (default for terminal output).

`-1`
:   List one file per line.

`-F`, `--classify`
:   Append indicator: `*` executable, `/` dir, `@` symlink.

`--colorful`
:   Multi-color output (eza/lsd style).

`--icons`
:   Display icons before file names (lsd style).

`--color`[=*WHEN*]
:   Colorize output: `always`, `auto`, or `never`. Default: `auto`.

## Sorting

`-r`, `--reverse`
:   Reverse order when sorting.

`-U`
:   Do not sort; list entries in directory order.

## Sizing

`--block-size`=*SIZE*
:   Scale sizes by SIZE (e.g., `K` for KiB, `M` for MiB).

## Other

`--tui`
:   Interactive file browser (falls back to normal output without a TTY).

`--json`
:   Output in JSON format.

`-h`, `--help`
:   Display help and exit.

# EXAMPLES

```bash
modbox ls
modbox ls -l
modbox ls -la
modbox ls -lh
modbox ls -lA
modbox ls -F
modbox ls -1
modbox ls -r
modbox ls -t -r
modbox ls -lh --block-size=K
modbox ls --color=always
modbox ls src/ include/
modbox ls -d */
modbox ls --icons --colorful
```

# NOTES

- `-a` implies `-A` is ignored — both list hidden files.
- When output is not a terminal (e.g., piped), color is disabled unless `--color=always`.
- `--color` (with no value) is treated as `--color=always` for convenience.
- Multiple directory arguments are supported; each is listed in turn.
- The `--tui` option requires a TTY; it falls back to normal output when stdout is not a terminal.
- The `--colorful` and `--icons` options imply `--color=always`.

# EXIT STATUS

`0` on success, `1` on error.

# SEE ALSO

**modbox-cat**(1), **modbox-rm**(1), **modbox**(1)