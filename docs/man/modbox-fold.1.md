% MODBOX-FOLD(1) modbox | User Commands
% modbox project
% 2026-08-23

# NAME

modbox-fold - wrap input lines to specified width

# SYNOPSIS

**modbox fold** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Wrap input lines in each FILE (or standard input), writing to standard output.

If no FILE is given, read from standard input.

# OPTIONS

## Standard options

`-b`, `--bytes`
:   Use `MAX` bytes instead of columns.

`-s`, `--spaces`
:   Break lines at spaces rather than in the middle of words.

`-w`, `--width`=*WIDTH*
:   Set output width to `WIDTH` columns. Default is 80.

`-h`, `--help`
:   Display help and exit.

`--version`
:   Output version information and exit.

# EXAMPLES

```bash
modbox fold text.txt
modbox fold -w 72 text.txt
modbox fold -s text.txt
modbox fold -w 40 -s /etc/hosts | column -t
cat long_line.txt | modbox fold -w 80 > wrapped.txt
```

# NOTES

* `-b`/`--bytes` changes the unit from columns to bytes; useful for fixed-width byte streams.
* `-s`/`--spaces` avoids breaking words but may produce shorter lines than requested when no space is found.
* Input is processed line by line; lines are broken after they are read.

# EXIT STATUS

`0` on success, `1` on error.

# SEE ALSO

**modbox-head**(1), **modbox-tail**(1), **modbox-cat**(1), **modbox-wc**(1), **modbox**(1)
