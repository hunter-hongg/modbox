% MODBOX-CUT(1) modbox | User Commands
% modbox project
% 2026-08-07

# NAME

modbox-cut - remove sections from each line of files

# SYNOPSIS

**modbox cut** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Print selected parts of lines from each FILE to standard output.
With no FILE, or when FILE is `-`, read from standard input.

Exactly one of **-b** (bytes), **-c** (characters), or **-f** (fields)
must be specified.  Each selects a different notion of "part" from each
input line.

# OPTIONS

**-b**, **--bytes=LIST**
:   Select only these bytes of each line.

**-c**, **--characters=LIST**
:   Select only these characters of each line.
    In the current implementation, byte and character selection are
    identical (single-byte locale).

**-d**, **--delimiter=DELIM**
:   Use DELIM instead of TAB as the field delimiter.
    Common escapes: `\t` (TAB), `\n` (newline), `\0` (NUL).
    Requires **-f** (field mode).

**-f**, **--fields=LIST**
:   Select only these fields of each line.  Fields are delimited by
    TAB by default, or by the character given with **-d**.

**--complement**
:   Complement the set of selected bytes, characters, or fields.
    Instead of keeping the specified ranges, exclude them and keep
    everything else.

**-s**, **--only-delimited**
:   Do not print lines that do not contain the delimiter.
    Requires **-f** (field mode).

**--output-delimiter=STRING**
:   Use STRING as the output delimiter instead of the input delimiter.
    Useful when rearranging fields with a different separator.

**-z**, **--zero-terminated**
:   Line delimiter is NUL (`\0`) instead of newline.
    Useful with `find -print0` and `xargs -0`.

**-h**, **--help**
:   Display help and exit.

# LIST SYNTAX

Each LIST is made up of one range or many ranges separated by commas.
Ranges are 1-indexed (the first byte, character, or field is number 1).

`N`
:   The N'th byte, character, or field.

`N-`
:   From the N'th byte, character, or field to the end of the line.

`N-M`
:   From the N'th to the M'th (inclusive) byte, character, or field.

`-M`
:   From the first to the M'th (inclusive) byte, character, or field.

Ranges may be combined with commas: `1,3-5,7-`.
Overlapping or adjacent ranges are merged automatically.

# NOTES

- Exactly one of **-b**, **-c**, **-f** must be specified.
  Using more than one is an error.
- **-d** and **-s** require **-f** (field mode); they are meaningless
  in byte or character mode.
- The default field delimiter is TAB.
- **-n** is accepted but ignored for POSIX compatibility.
- With **--complement**, a range like `-f 2` keeps all fields except
  field 2; `-f 1,3` on a 3-field line keeps only field 2.

## Differences from GNU cut

Not implemented: `--complement` with `-b` on multi-byte characters
(multi-byte locale support), `--output-delimiter` with multi-byte
characters, `--only-delimited` with `-b`/`-c`.

# EXAMPLES

```bash
# Extract the third field from a colon-separated file
modbox cut -d: -f3 /etc/passwd

# Get bytes 1 through 10 of a file
modbox cut -b 1-10 data.bin

# Get characters from position 5 to end
modbox cut -c 5- letters.txt

# Extract fields 1, 3, and 5 from CSV
modbox cut -d, -f 1,3,5 data.csv

# Complement: output everything except field 2
modbox cut -d, --complement -f 2 data.csv

# Only print lines that contain a delimiter
modbox cut -d: -s -f 1 /etc/passwd

# Use a different output delimiter
modbox cut -d, -f 1,3 --output-delimiter=$'\t' data.csv

# NUL-terminated output
modbox cut -d: -f 1 -z /etc/passwd
```

# EXIT STATUS

`0`
:   Successful execution.

non-zero
:   An error occurred (e.g., invalid LIST syntax, missing operand).

# SEE ALSO

**modbox-awk**(1), **modbox-sed**(1), **modbox-sort**(1), **modbox**(1)
