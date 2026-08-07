% MODBOX-DIFF(1) modbox | User Commands
% modbox project
% 2026-08-07

# NAME

modbox-diff - compare files line by line

# SYNOPSIS

**modbox diff** [*OPTION*]... *FILE1* *FILE2*

# DESCRIPTION

Compare *FILE1* and *FILE2* line by line and write a diff to standard
output.  If either file is `-`, input is read from standard input.

By default, diff outputs in the normal format.  Use **-u** for unified
format (the de facto standard for patches) or **-c** for context format.

# OPTIONS

**-q**, **--brief**
:   Report only whether the files differ.  No diff content is produced.
    Exit status is `0` if files are identical, `1` if they differ.

**-s**, **--report-identical-files**
:   Report when two files are identical.
    Without this flag, identical files produce no output.

**-i**, **--ignore-case**
:   Ignore case differences.  Characters differing only in case are
    treated as equal.

**-w**, **--ignore-all-space**
:   Ignore all whitespace.  All runs of whitespace are treated as a
    single space, and leading/trailing whitespace is ignored.

**-b**, **--ignore-space-change**
:   Ignore changes in the amount of whitespace.  Whitespace differences
    are ignored only when they occur within an otherwise unchanged line.
    This is less aggressive than **--ignore-all-space**.

**-p**, **--show-c-function**
:   In unified and context formats, show the name of the nearest C
    function above each change.

**-t**, **--expand-tabs**
:   Expand tabs to spaces in the output to make alignment clearer.

**-u**, **--unified**
:   Output unified diff format with 3 lines of context.
    This is the most commonly used format; it is compatible with
    `patch -p1` and is the default for `git diff`.

**-U**, **--unified-context=LINES**
:   Like **--unified**, but with *LINES* lines of context instead of
    the default 3.

**-c**, **--context**
:   Output context diff format with 3 lines of context.

**-C**, **--context-context=LINES**
:   Like **--context**, but with *LINES* lines of context.

**--color=WHEN**
:   Colorize the output.  WHEN can be `always`, `auto` (enabled only
    when stdout is a terminal, which is the default behavior), or
    `never`.

**--normal**
:   Output a normal diff (the default format).

**-h**, **--help**
:   Display help and exit.

# OUTPUT FORMATS

## Normal format

The default format uses Ed-style edit commands:

```
1,3c1,3       — lines 1-3 of FILE1 are replaced by lines 1-3 of FILE2
< old line    — line from FILE1 (old)
> new line    — line from FILE2 (new)
---
```

`a` means "append after", `d` means "delete", `c` means "change".

## Unified format

The unified format shows a compact view with `@@` hunk headers:

```
--- file1
+++ file2
@@ -1,3 +1,4 @@
 context
-old line
+new line 1
+new line 2
 context
```

Lines prefixed with a space are unchanged context.  `-` lines are
removed from FILE1; `+` lines are added from FILE2.

## Context format

The context format shows surrounded blocks:

```
***************
*** 1,3 ****   — old file lines
  context
- old line
--- 1,4 ----   — new file lines
  context
+ new line 1
+ new line 2
```

# NOTES

- **-b** (ignore-space-change) is less aggressive than **-w**
  (ignore-all-space): `-b` only ignores whitespace changes within a
  line that is otherwise unchanged, while `-w` ignores all whitespace
  everywhere.
- When no format flag is given, the normal format is used.
- `--color=auto` (the default) enables color only when stdout is a
  terminal.
- The diff engine uses an LCS-based algorithm internally.

## Differences from GNU diff

Not implemented: `-l` (pass through `pr`), `-e` (ed script),
`--rcs` (RCS format), `--side-by-side`, `--horizon-lines`,
`--strip-trailing-cr`, `-S` (start with file), `--label`,
directory recursion (`-r` is in modbox's `find` command).

# EXAMPLES

```bash
# Basic diff (normal format)
modbox diff file1.txt file2.txt

# Unified diff (most common)
modbox diff -u file1.txt file2.txt > patch.diff

# Brief: only report whether they differ
modbox diff -q file1.txt file2.txt

# Ignore whitespace differences
modbox diff -w file1.txt file2.txt

# Ignore case
modbox diff -i file1.txt file2.txt

# Unified diff with 5 lines of context
modbox diff -U 5 file1.txt file2.txt

# Context diff with 10 lines of context
modbox diff -C 10 file1.txt file2.txt

# Colorize output
modbox diff --color=always -u file1.txt file2.txt

# Report identical files
modbox diff -s file1.txt file2.txt
```

# EXIT STATUS

`0`
:   Files are identical (or no differences found).

`1`
:   Files differ (or an error occurred with **--brief**).

`2`
:   An error occurred (e.g., file not found, I/O error).

# SEE ALSO

**modbox-diff3**(1), **modbox-patch**(1), **modbox**(1)
