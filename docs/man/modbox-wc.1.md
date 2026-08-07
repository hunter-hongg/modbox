% MODBOX-WC(1) modbox | User Commands
% modbox project
% 2026-08-07

# NAME

modbox-wc - print newline, word, and byte counts

# SYNOPSIS

**modbox wc** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Print newline, word, and byte counts for each FILE, and a total line
if more than one FILE is given.
With no FILE, or when FILE is `-`, read from standard input.

By default, all three counts (lines, words, bytes) are printed.
Specify individual flags to print only the desired counts.

# OPTIONS

**-c**, **--bytes**
:   Print the byte counts.

**-m**, **--chars**
:   Print the character counts.
    In the current implementation, this counts bytes (same as
    **--bytes**), as multi-byte character counting is not yet
    supported.

**-l**, **--lines**
:   Print the newline counts.

**-w**, **--words**
:   Print the word counts.  Words are sequences of non-whitespace
    characters separated by whitespace (space, tab, newline, carriage
    return, form feed, vertical tab).

**--json**
:   Output the results in JSON format instead of the default
    human-readable table.
    Each file is represented as a JSON object with `bytes`, `chars`,
    `lines`, `name`, and `words` fields.
    With multiple files, a final `"total"` entry is included.
    This is a modbox extension.

**-h**, **--help**
:   Display help and exit.

# OUTPUT FORMAT

With one or more files, the default output has one line per file:

```
   lines  words  bytes  filename
```

With multiple files, an additional `total` line is printed:

```
   lines  words  bytes  file1
   lines  words  bytes  file2
   lines  words  bytes  total
```

Fields are right-justified in 7-character columns.
When reading from stdin with no filename, only the counts are printed
(without a filename column).

# NOTES

- With no option flags, the default is `-l -w -c` (lines, words,
  bytes).
- Short options can be combined: `wc -lwm file` is equivalent to
  `wc -l -w -m file`.
- **--bytes** and **--chars** both count raw bytes in the current
  implementation; there is no distinction between byte count and
  character count for multi-byte encodings.
- A "word" is a sequence of non-whitespace characters.  Empty files
  have zero words.
- Whitespace is defined as: space, tab, newline, carriage return,
  form feed, and vertical tab.

## Differences from GNU wc

Not implemented: `--files0-from=F` (read file names from F, NUL
separated), `--max-lines=N` (stop after N lines), wide character
support for `--chars` (currently counts bytes).

# EXAMPLES

```bash
# Count lines, words, bytes in a file
modbox wc file.txt

# Count lines only
modbox wc -l file.txt

# Count words and bytes
modbox wc -w -c file.txt

# Combined short flags
modbox wc -lwm file.txt

# Count from stdin
echo "hello world" | modbox wc

# Multiple files with total
modbox wc file1.txt file2.txt file3.txt

# JSON output
modbox wc --json file1.txt file2.txt
```

# EXIT STATUS

`0`
:   Successful execution.

non-zero
:   An error occurred (e.g., file not found).

# SEE ALSO

**modbox-cut**(1), **modbox-awk**(1), **modbox-sort**(1), **modbox**(1)
