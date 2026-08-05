% MODBOX-SORT(1) modbox | User Commands
% modbox project
% 2026-08-05

# NAME

modbox-sort - sort lines of text files

# SYNOPSIS

**modbox sort** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Sort lines of text files. With no FILE, or when FILE is `-`,
read from standard input. Output is written to standard output
or to the file specified with `-o`.

# OPTIONS

`-b`, `--ignore-leading-blanks`
:   Ignore leading blanks when sorting.

`-f`, `--ignore-case`
:   Fold lower case to upper case; case is ignored in comparisons.

`-n`, `--numeric-sort`
:   Compare according to string numerical value.

`-r`, `--reverse`
:   Reverse the result of comparisons.

`-u`, `--unique`
:   With `-c`, check for strict ordering (no duplicates allowed).
    Without `-c`, output only the first of an equal run.

`-c`, `--check`
:   Check whether input is sorted. Do not sort; exit with error
    if any line is out of order.

`-s`, `--stable`
:   Stabilize sort by disabling last-resort comparison
    (compare entire lines when all keys are equal).

`-k`, `--key=POS1[,POS2]`
:   Sort via a key. POS is `F[.C][FLAGS]`.
    Multiple `-k` flags define primary, secondary, and tertiary keys.

`-t`, `--field-separator=SEP`
:   Use SEP as the field separator instead of the default
    (transition from non-blank to blank characters).

`-o`, `--output=FILE`
:   Write the sorted result to FILE instead of standard output.

`-h`, `--help`
:   Display help and exit.

### Key Position Format

POS has the form `F[.C][FLAGS]`:

- `F` — field number (1-indexed)
- `.C` — character offset within the field (1-indexed)
- `FLAGS` — optional key flags (any combination):

`b`
:   Ignore leading blanks in the key.

`f`
:   Ignore case within the key.

`n`
:   Numeric comparison within the key.

`r`
:   Reverse comparison within the key.

# EXAMPLES

```bash
modbox sort file.txt
modbox sort -r file.txt
modbox sort -n numbers.txt
modbox sort -t, -k2 data.csv
modbox sort -k1,1 -k2,2n data.csv
modbox sort -b -f mixed.txt
modbox sort -u sorted.txt
modbox sort -c sorted.txt
modbox sort -s stable.txt
modbox sort -o output.txt input.txt
modbox sort -t: -k3,3n /etc/passwd
```

# NOTES

- The default sort locale is the C locale (byte-value order), not the
  system locale. Sort order may differ from `ls` or shell globbing.
- Multiple `-k` flags define a cascade of sort keys (primary, secondary, etc.).
- Without `-o`, output goes to standard output.
- `-c` and `-u` together check for strict ordering (no duplicate lines).
- The key flags (b, f, n, r) are appended directly after the position
  in a `-k` argument (e.g., `-k2,2n` sorts field 2 numerically).
- Empty lines sort before all non-empty lines in the default order.

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

**modbox-head**(1), **modbox-tail**(1), **modbox**(1)
