% MODBOX-GREP(1) modbox | User Commands
% modbox project
% 2026-08-06

# NAME

modbox-grep - search for patterns in files

# SYNOPSIS

**modbox grep** [*OPTION*]... PATTERN [*FILE*]...

# DESCRIPTION

Print lines matching a pattern to standard output.
With no FILE, or when FILE is `-`, read from standard input.
The pattern is matched using C++ `std::regex` (ECMAScript) syntax by default.
Use **-E** for extended regular expressions or **-F** for fixed strings.
When multiple files are given, the file name is prefixed to each match.

# OPTIONS

## Pattern selection

**-E**, **--extended-regexp**
:   Interpret PATTERN as an extended regular expression (ERE).

**-F**, **--fixed-strings**
:   Interpret PATTERN as a set of newline-separated fixed strings.

**-e**, **--regexp=PATTERN**
:   Use PATTERN as the pattern.
    Useful to protect patterns beginning with `-`.

## Matching control

**-i**, **--ignore-case**
:   Ignore case distinctions in both the pattern and the input.

**-v**, **--invert-match**
:   Select non-matching lines.

**-w**, **--word-regexp**
:   Force the pattern to match only whole words.

**-x**, **--line-regexp**
:   Force the pattern to match only whole lines.

## Output control

**-c**, **--count**
:   Print only a count of matching lines per FILE.

**-l**, **--files-with-matches**
:   Print only the names of files containing matches.

**-n**, **--line-number**
:   Print the line number of each matching line.

**-o**, **--only-matching**
:   Print only the matched (non-empty) parts of a matching line.

**-H**, **--with-filename**
:   Always print the file name with output lines.

**-h**, **--no-filename**
:   Suppress the file name prefix on output (when searching multiple files).

**--color=WHEN**
:   Highlight matching text with ANSI color codes.
    WHEN can be `always`, `auto` (default, enabled only on terminals), or `never`.
    `--color` with no value is treated as `--color=always`.

## Recursive search

**-r**, **--recursive**
:   Read all files under each directory recursively.

**-R**, **--dereference-recursive**
:   Like `-r`, but follow all symbolic links.

**-h**, **--help**
:   Display help and exit.

## Interactive mode

**--tui**
:   Open an interactive TUI viewer for the search results.
    Requires a TTY; falls back to normal output when stdout is not a terminal.
    This is a modbox extension not found in standard grep.

# EXIT STATUS

`0`
:   At least one line was selected.

`1`
:   No lines were selected.

`2`
:   An error occurred (e.g., invalid pattern, unreadable file).

# NOTES

- Basic regex uses C++ `std::regex` (ECMAScript) syntax.
- The `-e` flag is needed to protect patterns that begin with `-`.
- When a single file is given, the file name is not printed by default.
- When multiple files are given, the file name is always prefixed.
- `--color=auto` (the default) enables color only when output is a terminal.

## Differences from GNU grep

The following GNU grep features are **not implemented**:

- `-A`, `-B`, `-C` — context lines (after/before/around)
- `-P` — Perl-compatible regular expressions (PCRE)
- `-z`, `--null-data` — NUL-terminated lines
- `-b` — byte offset of match
- `-q`, `--quiet` — suppress all normal output
- `-s` — suppress error messages
- `-d ACTION` — how to handle directories
- `--include=GLOB`, `--exclude=GLOB` — file-name filtering
- `-m` COUNT, `--max-count=COUNT` — stop after N matches
- `--label=LABEL` — label for stdin
- `-Z`, `--null` — output NUL byte after file name

# EXAMPLES

```bash
# Search a file for a pattern
modbox grep "error" /var/log/syslog

# Extended regex
modbox grep -E "^[0-9]+\.[0-9]+" data.csv

# Recursive search ignoring case
modbox grep -r -i "deprecated" /etc/

# Invert match: show lines that do NOT match
modbox grep -v "^#" config.ini

# Print file names only (useful with pipelines)
modbox grep -l pattern /path/to/dir/

# Highlight matches in color
modbox grep --color=always "error" file.txt

# Read from stdin
echo "hello world" | modbox grep -i hello
```

# SEE ALSO

**modbox-sed**(1), **modbox-awk**(1), **modbox-find**(1), **modbox**(1)
