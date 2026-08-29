% MODBOX-LESS(1) modbox | User Commands
% modbox project
% 2026-08-29

# NAME

modbox-less - paginate or view files in a terminal pager

# SYNOPSIS

**modbox less** [*OPTION*]... [*FILE*]...

# DESCRIPTION

**less** is a terminal pager that displays the contents of one or more
files (or standard input) one screen at a time. It supports forward and
backward scrolling, regular-expression search, line numbers, and a
GNU-style progress prompt.

With no FILE, or when FILE is `-`, standard input is read.

When output is not a terminal (a pipe, file redirection, or a test
harness), **less** behaves like **cat**, printing all input and exiting
immediately — no interactive prompt is shown.

# OPTIONS

**-N**, **--LINE-NUMBERS**
:   Display a line number at the start of each line.

**-i**
:   Ignore case in search patterns.

**-I**
:   Force case-insensitive search, even when the pattern contains
    uppercase letters.

**-M**
:   Use a verbose (long) prompt showing the filename, the current line
    range, the total number of lines, and the scroll percentage.

**-E**, **--QUIT-AT-EOF**
:   Quit immediately when an attempt is made to scroll past the end of
    the last file.

**-F**, **--quit-if-one-screen**
:   Quit if the entire input fits on one screen.

**-X**
:   Do not clear the screen on startup and exit, leaving the displayed
    text on the terminal.

**-S**, **--chop-long-lines**
:   Chop (truncate) long lines at the screen width rather than wrapping
    them.

**-p** *PATTERN*, **--pattern=** *PATTERN*
:   Start at the first line matching *PATTERN*.

**-h**, **--help**
:   Display help and exit.

**--version**
:   Display version information and exit.

# STARTUP COMMANDS

A startup command may be given with **+cmd**:

- **+G** — start at the end of the file.
- **+N** — start at line number N.
- **+/PATTERN** — start at the first line matching PATTERN (equivalent
  to **-p** *PATTERN*).

# KEYS (interactive mode)

- **SPACE**, **PageDown**, **f** — forward one screen.
- **b**, **PageUp** — backward one screen.
- **j**, **Down**, **e**, **CR** — forward one line.
- **k**, **Up**, **y** — backward one line.
- **g**, **<** — go to the first line.
- **G**, **>** — go to the last line.
- **/** *PATTERN* — forward search.
- **?** *PATTERN* — backward search.
- **n** — next match (same direction).
- **N** — previous match (opposite direction).
- **q** — quit.

# EXIT STATUS

`0`
:   Successful execution.

`1`
:   An error occurred (e.g., file not found).

`2`
:   Invalid usage or unrecognized option.

# SEE ALSO

**modbox-cat**(1), **modbox-tee**(1), **modbox**(1)
