% MODBOX-COLUMN(1) modbox | User Commands
% modbox project
% 2026-09-06

# NAME

modbox-column - format data into columns

# SYNOPSIS

**modbox column** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Format data into columns. With no *FILE*, or when *FILE* is `-`, read from
standard input.

By default (fill mode), the input lines are distributed into as many columns as
fit in the output width, filling down each column before moving to the next.
With **-t** (table mode), the fields of each line are aligned into columns
padded to their maximum width.

# OPTIONS

## Table options

`-t`, `--table`
:   Make the output a table (align the fields of each line into columns).

`-s`, `--separator=CHAR`
:   Use *CHAR* as the input field separator. By default a run of whitespace
    (spaces and tabs) separates fields.

`-c`, `--width=WIDTH`
:   Set the output width to *WIDTH* characters. In fill mode this controls the
    number of columns. Default is 80.

`-o`, `--table-output=FILE`
:   Send the output to *FILE* instead of standard output.

`-N`, `--table-column-names=TEXT`
:   Override the column names with the comma-separated *TEXT*. A component
    equal to `-` keeps the value from the first input row.

`-r`, `--table-right-justified`
:   Right-align all columns.

`-R`, `--table-right`
:   Right-align all columns (equivalent to `-r`).

`-C`, `--table-rightmost`
:   Right-align only the rightmost column; the others stay left-aligned.

`-d`, `--table-divider`
:   Insert a divider (an extra space) between columns.

`-L`, `--table-indent=TEXT`
:   Indent every output line with *TEXT*.

`-a`, `--table-full`
:   Allow full output (no width constraint).

`-e`, `--table-entry=TEXT`
:   Use *TEXT* as the separator between table cells (for multi-line cells).

`-l`, `--table-lines=NUM`
:   Fold the table into groups of *NUM* physical lines, each group forming a
    single logical record (multi-line cells).

`-H`, `--table-print-head`
:   Treat the first row as a header and repeat it before every group of 25
    data rows.

## Standard options

`-h`, `--help`
:   Display help and exit.

`--version`
:   Output version information and exit.

# EXAMPLES

```bash
modbox column -t /etc/fstab
modbox column -s, -t data.csv
ls -l | modbox column -t
seq 20 | modbox column -c 60
modbox column -t -r numbers.txt
modbox column -t -N name,age city.txt
```

# NOTES

* Column widths are measured in characters (not display cells); trailing
  whitespace on a field does not count toward its width.
* The rightmost column on each row is not right-padded, so rows carry no
  trailing whitespace.
* Fill mode computes the number of columns as the largest count whose columns
  fit within the output width, then fills down each column first.
* When several files are given, each is formatted independently and the results
  are concatenated; no separator is inserted between them.

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

**modbox-fmt**(1), **modbox-fold**(1), **modbox-pr**(1), **modbox-cut**(1), **modbox**(1)
