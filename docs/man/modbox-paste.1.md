% MODBOX-PASTE(1) modbox | User Commands
% modbox project
% 2026-08-11

# NAME

modbox-paste - merge lines of files

# SYNOPSIS

**modbox paste** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Merge corresponding lines from each FILE, separated by TABs, and write
the result to standard output. With no FILE, or when FILE is **\-**,
read from standard input.

# OPTIONS

**-d**, **--delimiters=LIST**
:   Use characters from LIST as delimiters instead of TABs. Characters
    are reused cyclically if LIST is shorter than the number of files.

**-s**, **--serial**
:   Paste one file at a time instead of in parallel. Each file's lines
    are printed sequentially rather than merged column-wise.

**-z**, **--zero-terminated**
:   Use NUL (`\0`) as the line delimiter instead of newline.

**-h**, **--help**
:   Display help and exit.

**--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Merge two files column-wise (default TAB separator)
modbox paste file1.txt file2.txt

# Use custom delimiters
modbox paste -d ',' file1.txt file2.txt

# Cycle delimiters for three files
modbox paste -d ':-' a.txt b.txt c.txt

# Serial mode: print one file at a time
modbox paste -s file1.txt file2.txt

# NUL-terminated output
modbox paste -z file1.txt file2.txt | xargs -0
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- By default, files are merged in parallel with TAB separators.
- When **-d** is given with fewer characters than files, the character
  list is reused cyclically. For example, with three files and **-d** `,`
  the delimiters cycle as `,`, `,`, `,`.
- **-s** (serial) is useful when you want to concatenate all lines of
  each file into a single line before moving to the next file.

# SEE ALSO

**modbox-cut**(1), **modbox-join**(1), **modbox-unexpand**(1), **modbox**(1)
