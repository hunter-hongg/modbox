% MODBOX-COMM(1) modbox | User Commands
% modbox project
% 2026-08-11

# NAME

modbox-comm - compare two sorted files line by line

# SYNOPSIS

**modbox comm** [*OPTION*]... FILE1 FILE2

# DESCRIPTION

Compare two sorted files line by line and produce a three-column output.
Column one contains lines unique to FILE1, column two contains lines
unique to FILE2, and column three contains lines common to both files.
With no options, all three columns are shown, separated by tabs.

# OPTIONS

**-1**
:   Suppress column one (lines unique to FILE1).

**-2**
:   Suppress column two (lines unique to FILE2).

**-3**
:   Suppress column three (lines common to both).

**-i**, **--ignore-case**
:   Ignore case when comparing lines.

**--check-order**
:   Check that the input is correctly sorted; exit with an error if not.

**--nocheck-order**
:   Do not check whether the input is sorted (the default).

**--output-delimiter=STR**
:   Use STR as the column separator instead of the default tab character.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Compare two sorted files, show all columns
modbox comm file1.txt file2.txt

# Show only lines unique to file1
modbox comm -2 -3 file1.txt file2.txt

# Show only common lines
modbox comm -1 -2 file1.txt file2.txt

# Case-insensitive comparison
modbox comm -i sorted_a.txt sorted_b.txt

# Custom output delimiter
modbox comm --output-delimiter=',' file1.txt file2.txt
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- FILE1 and FILE2 **must be sorted** for correct results. Use
  **modbox-sort**(1) if they are not already sorted.
- Without any suppression flags, the output has three tab-separated
  columns: unique-to-file1, unique-to-file2, and common.
- `--check-order` verifies that lines appear in sorted order; use it in
  scripts to catch unsorted input early.

# SEE ALSO

**modbox-sort**(1), **modbox-diff**(1), **modbox-uniq**(1), **modbox**(1)
