% MODBOX-UNIQ(1) modbox | User Commands
% modbox project
% 2026-08-07

# NAME

modbox-uniq - report or omit repeated lines

# SYNOPSIS

**modbox uniq** [*OPTION*]... [*INPUT* [*OUTPUT*]]

# DESCRIPTION

Filter adjacent matching lines from INPUT (or standard input), writing
to OUTPUT (or standard output).  To find duplicates anywhere in a file
(not just adjacent), pipe through **sort** first.

With no options, matching lines are merged to the first occurrence.

# OPTIONS

**-c**, **--count**
:   Prefix each output line with the number of times it occurred in
    the input, right-justified in a 7-character field.

**-d**, **--repeated**
:   Print only duplicate lines, one per group of identical adjacent
    lines.  Unique lines are suppressed.

**-D**, **--all-repeated**
:   Print all lines from duplicate groups (all occurrences, not just
    one).  Unlike **-d**, every line in a duplicate run is output.

**-u**, **--unique**
:   Print only unique lines (lines that do not repeat adjacently).
    Duplicate lines are suppressed entirely.

**-i**, **--ignore-case**
:   Ignore case when comparing lines.  Lines that differ only in
    case are treated as duplicates.

**-f**, **--skip-fields=N**
:   Avoid comparing the first N fields of each line.
    Fields are whitespace-delimited.

**-s**, **--skip-chars=N**
:   Avoid comparing the first N characters of each line.

**-w**, **--check-chars=N**
:   Compare no more than N characters per line.  Useful for ignoring
    trailing differences.

**-h**, **--help**
:   Display help and exit.

# OUTPUT BEHAVIOUR

The combination of **-c**, **-d**, **-D**, and **-u** determines what
is emitted:

| Option    | Behaviour                                       |
|-----------|--------------------------------------------------|
| (none)    | Print each group once (merge adjacent duplicates) |
| **--count**   | Print count prefix before each group's first line |
| **--repeated**  | Print only groups with more than one line        |
| **--all-repeated** | Print every line of every duplicate group     |
| **--unique**    | Print only groups with exactly one line        |

These flags are mutually exclusive in their effect; combining them
produces undefined behaviour.

# NOTES

- Uniqueness is determined by adjacency: only consecutive identical
  lines are considered duplicates.  Use `sort | uniq` to deduplicate
  an entire file.
- **-f** (skip fields) and **-s** (skip characters) are applied
  before **-w** (check chars).
- `--all-repeated` without a modifier prints all duplicates.  The
  `--all-repeated=` variants (prepend, repeat, none) are not
  implemented.
- Fields are whitespace-delimited (runs of whitespace separate fields).

## Differences from GNU uniq

Not implemented: `--all-repeated=PREPEND`, `--all-repeated=REPEAT`,
`--all-repeated=NONE`, `--group[=TYPE]` (annotate with group markers).

# EXAMPLES

```bash
# Remove adjacent duplicates
modbox uniq input.txt

# Count occurrences (classic sort | uniq -c)
modbox sort data.txt | modbox uniq -c

# Show only duplicated lines
modbox uniq -d input.txt

# Show only unique lines (no duplicates at all)
modbox uniq -u input.txt

# Case-insensitive deduplication
modbox uniq -i input.txt

# Skip first field when comparing
modbox uniq -f 1 data.txt

# Compare only first 10 characters
modbox uniq -w 10 input.txt

# Print all lines from duplicate groups
modbox uniq -D input.txt
```

# EXIT STATUS

`0`
:   Successful execution.

non-zero
:   An error occurred (e.g., unreadable input file).

# SEE ALSO

**modbox-sort**(1), **modbox-awk**(1), **modbox-grep**(1), **modbox**(1)
