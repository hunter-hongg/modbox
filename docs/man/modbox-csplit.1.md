% MODBOX-CSPLIT(1) modbox | User Commands
% modbox project
% 2026-08-21

# NAME

modbox-csplit - split a file into sections determined by context lines

# SYNOPSIS

**modbox csplit** [*OPTION*]... *FILE* *PATTERN*...

# DESCRIPTION

Split *FILE* into a sequence of zero or more sections, each determined by a
context line (line number or regular expression). Output files are named
PREFIX00, PREFIX01, etc. and written to the current directory.

If *FILE* is `-`, input is read from standard input. When no *FILE* is given,
all remaining arguments are treated as patterns applied to stdin.

By default, empty output files are kept. Use **-z** to remove them.

# OPTIONS

**-f**, **--prefix=PREFIX**
:   Use PREFIX as the output file name prefix instead of the default "xx".

**-b**, **--suffix-format=FORMAT**
:   Use sprintf FORMAT for the suffix instead of the default `%02d`.
    The format is applied to a zero-based counter for each output file.
    Example: `--suffix-format=%03x` produces PREFIX000, PREFIX001, ...

**-n**, **--digits=DIGITS**
:   Set the number of digits in the default suffix format.
    Default is 2. Ignored when **-b** is used.

**-z**, **--elide-empty-files**
:   Remove output files that contain zero lines after splitting.

**-s**, **--quiet**, **--silent**
:   Do not print the sizes of output files to standard output.

**-k**, **--keep-files**
:   Do not remove output files if an error occurs during processing.

**-h**, **--help**
:   Display help and exit.

# PATTERNS

Each PATTERN determines where a split occurs. Patterns are processed in order.

**LINE_NO**
:   Split at the 1-indexed line number. The matching line becomes the first
    line of the next output file.

**/REGEXP/[OFFSET]**
:   Split at the first line matching the POSIX extended regular expression.
    The matching line is included in the current section (offset 0) or
    excluded (offset -1). If no OFFSET is given, the default is 0.

**%REGEXP%[OFFSET]**
:   Like **/REGEXP/** but the matching line is excluded from both the
    current section and the next (treated as offset -1).

**{N}**
:   Repeat the immediately preceding pattern N more times.
    N must be a non-negative integer.

**{*}**
:   Repeat the immediately preceding pattern as many times as possible,
    until the end of the input is reached.

Offsets can be specified after a regex pattern as **/+N/** (include N lines
after match in current section) or **/-N/** (exclude N lines).

# OUTPUT FILE SIZES

By default, csplit prints the size in bytes of each output file to stdout,
one per line, in order. Use **-s** to suppress this output.

# EXIT STATUS

`0`
:   Successful splitting.

non-zero
:   An error occurred (e.g., invalid pattern, unreadable file, write error).

# NOTES

- The `{N}` and `{*}` syntax repeats the *previous* pattern, not a fixed count.
  For example, `5 {2}` outputs three files: lines 1-5, 6-10, 11 onwards.
- Regex patterns use the system regex library (POSIX ERE).
- On error, output files are removed by default unless **-k** is given.

## Differences from GNU csplit

Not implemented: `-q` (quiet, combined with `-s`), `%REGEXP%` syntax varies
slightly; GNU csplit supports `{n,m}` range repetition which modbox does not.

# EXAMPLES

```bash
# Split a file at lines 10 and 20
modbox csplit data.txt 10 20

# Split at a regex match
modbox csplit report.txt '/^##/'

# Split with custom prefix and digit count
modbox csplit -f segment -n 3 input.txt 100 200

# Elide empty files
modbox csplit -z long.txt '{*}'

# Keep files on error
modbox csplit -k input.txt 50 '/ERROR/'

# Read from stdin, split at blank lines
cat data.txt | modbox csplit - /blank/ '{*}'
```

# SEE ALSO

**modbox-split**(1), **modbox-awk**(1), **modbox-sed**(1), **modbox**(1)
