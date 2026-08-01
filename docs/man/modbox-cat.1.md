% MODBOX-CAT(1) modbox | User Commands
% modbox project
% 2026-08-02

# NAME

modbox-cat - concatenate files and print to standard output

# SYNOPSIS

**modbox cat** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Concatenate FILE(s) to standard output. With no FILE, or when FILE is `-`,
read from standard input.

# OPTIONS

## Standard options

`-b`, `--number-nonblank`
:   Number nonempty output lines.

`-E`, `--show-ends`
:   Display `$` at end of each line.

`-n`, `--number`
:   Number all output lines.

`-s`, `--squeeze-blank`
:   Suppress repeated empty lines (collapse runs of blank lines to one).

`-T`, `--show-tabs`
:   Display TAB characters as `^I`.

`-v`, `--show-nonprinting`
:   Use `^` and `M-` notation for non-printing characters (except LFD and TAB).

`-e`
:   Equivalent to `-vE`.

`-t`
:   Equivalent to `-vT`.

`-A`, `--show-all`
:   Equivalent to `-vET`.

`--less`
:   Pager mode (interactive: `j` next, `k` previous, `q` quit).

`--tui`
:   Interactive TUI viewer with tabs (requires TTY).

`-h`, `--help`
:   Display help and exit.

## Dev-tool options

`--blame`
:   Prefix each line with git blame info (`commit author date`).

`--highlight`
:   Syntax-highlight output based on file extension (auto, TTY only).

`--header`
:   Print a file metadata banner before the content.

`--diff`=*FILE*
:   Print unified diff between the input file and FILE.

## Content navigation

`--range`=*N-M*
:   Show only lines N through M (1-indexed, inclusive).

`--grep`=*PATTERN*
:   Keep only lines matching the extended regex PATTERN.

`--context`=*N*
:   With `--grep`, also show N context lines around each match.

`--head`=*N*
:   Show only the first N lines.

`--tail`=*N*
:   Show only the last N lines.

`--number-format`=*FMT*
:   Line-number format: `decimal` (default), `hex`, or `octal`.

`--stats`
:   Print line / word / character counts after the output.

# EXAMPLES

```bash
modbox cat file.txt
modbox cat -n file.txt
modbox cat -E file.txt
modbox cat -A file.txt
modbox cat --blame main.c
modbox cat --highlight --range=10-30 main.c
modbox cat --grep='TODO' --context=2 changelog.md
modbox cat --head=20 --tail=10 big.log
modbox cat --less long.txt
modbox cat a.txt b.txt > combined.txt
echo "hello" | modbox cat
```

# NOTES

- Short options can be combined: `-vET` is equivalent to `-A`.
- `--grep` and `--range` apply after the file is read; the rest of the
  pipeline (`--squeeze-blank`, `--head`, `--tail`) chains in order.
- `--diff`, `--grep`, `--range`, `--head`, `--tail` cannot be combined
  freely — they operate on the line stream and may filter each other out.
- The `--tui` and `--less` options require a TTY; they fall back to normal
  output when stdout is not a terminal.
- The `--highlight` option only works when stdout is a TTY.

# EXIT STATUS

`0` on success, `1` on error.

# SEE ALSO

**modbox-ls**(1), **modbox-rm**(1), **modbox**(1)