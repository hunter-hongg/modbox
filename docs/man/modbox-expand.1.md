% MODBOX-EXPAND(1) modbox | User Commands
% modbox project
% 2026-08-02

# NAME

modbox-expand - convert tabs to spaces

# SYNOPSIS

**modbox expand** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Convert tabs in each FILE to spaces, writing to standard output. With no FILE, or when FILE is `-`, read standard input.

# OPTIONS

**-i**, **--initial**
:   Do not convert tabs after non-blanks.

**-t**, **--tabs**=*N*
:   Use tab stops every N characters. A single number specifies interval mode; a comma-separated list specifies explicit stops. Default is 8.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
modbox expand file.txt
modbox expand -t 4 file.txt
modbox expand -i -t 8 < input
cat file | modbox expand -t 2,4,8
```

# NOTES

- When `-t` is a single number, tabs are expanded to the next multiple of N.
- With `--initial`, only leading tabs are expanded.

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

**modbox-unexpand**(1), **modbox**(1)
