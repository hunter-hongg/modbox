% MODBOX-RG(1) modbox | User Commands
% modbox project
% 2026-08-16

# NAME

modbox-rg - recursively search files for a pattern

# SYNOPSIS

**modbox rg** [*OPTION*]... **PATTERN** [*PATH*]...

# DESCRIPTION

Recursively search files for **PATTERN** using regular expressions.
Behavior is similar to [ripgrep](https://github.com/BurntSushi/ripgrep).

By default, the search is recursive, case-insensitive with smart-case,
and line numbers are shown. The pattern is interpreted as a regular
expression using C++ `std::regex` (ECMAScript) syntax.

# OPTIONS

## Pattern selection

**-e**, **--regexp=PATTERN**
:   Use **PATTERN** as the search pattern.
    Useful to protect patterns beginning with `-`.

**-F**, **--fixed-strings**
:   Treat **PATTERN** as a literal string, not a regex.

**-E**, **--extended-regexp**
:   Interpret **PATTERN** as an extended regular expression (ERE).

## Matching control

**-i**, **--ignore-case**
:   Perform case-insensitive matching.

**-s**, **--case-sensitive**
:   Force case-sensitive matching.

**-S**, **--smart-case**
:   Case-insensitive when the pattern is all lowercase (default).

**-v**, **--invert-match**
:   Select non-matching lines.

**-w**, **--word-regexp**
:   Match only whole words.

**-x**, **--line-regexp**
:   Match only whole lines.

## Output control

**-n**, **--line-number**
:   Show line numbers (default).

**-N**, **--no-line-number**
:   Suppress line numbers.

**-c**, **--count**
:   Print only a count of matches per file.

**-l**, **--files-with-matches**
:   Print only the names of files containing matches.

**-o**, **--only-matching**
:   Show only the matched part of each line.

**--color=WHEN**
:   Highlight matching text with ANSI color codes.
    WHEN can be `always`, `auto` (default), or `never`.
    `--color` with no argument is treated as `--color=always`.

## Context lines

**-C**, **--context=NUM**
:   Show NUM lines before and after each match.

**-A**, **--after-context=NUM**
:   Show NUM lines after each match.

**-B**, **--before-context=NUM**
:   Show NUM lines before each match.

## Filtering

**-g**, **--glob=GLOB**
:   Include or exclude files matching the glob pattern.
    Use `!PATTERN` to exclude.

**--hidden**
:   Search hidden files and directories.

**--max-depth=NUM**
:   Descend at most NUM directories deep.

**-m**, **--max-count=NUM**
:   Stop after NUM matches per file.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Basic recursive search
modbox rg "error" /project

# Case-sensitive search
modbox rg -s "FatalException" src/

# Search for literal string
modbox rg -F "not-a-regex" .

# Show context lines
modbox rg -C 3 "TODO" .

# Find files with matches
modbox rg -l "FIXME" /src

# Count matches per file
modbox rg -c "TODO" .

# Search with inverted match
modbox rg -v "^//" config.cpp

# Limit search depth
modbox rg --max-depth 2 "pattern" .

# Search hidden files
modbox rg --hidden "\\.env" .

# Glob filter
modbox rg -g "*.rs" "fn main" .
```

# NOTES

- Smart-case (default): patterns with no uppercase are matched case-insensitively;
  patterns with any uppercase are matched case-sensitively.
- The `-e` flag is needed to protect patterns that begin with `-`.
- When output is not a terminal (e.g., piped), color is disabled unless
  `--color=always` is set.

## Differences from ripgrep

The following ripgrep features are **not implemented**:

- `-P` — Perl-compatible regular expressions (PCRE2)
- `--json` — JSON output format
- `--stats` — print statistics
- `-u` / `--unrestricted` — search ignored/virtual files
- `--pretty` — pretty-printed output with highlighted matches

# EXIT STATUS

`0`
:   At least one match was found.

`1`
:   No match was found.

`2`
:   An error occurred (e.g., invalid pattern, unreadable file).

# SEE ALSO

**modbox-grep**(1), **modbox-fd**(1), **modbox-sed**(1), **modbox-awk**(1), **modbox**(1)
