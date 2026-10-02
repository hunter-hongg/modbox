% MODBOX-STRINGS(1) modbox | User Commands
% modbox project
% 2026-09-28

# NAME

modbox-strings - print the printable strings in a file

# SYNOPSIS

**modbox strings** [*OPTION*]... [FILE]...

# DESCRIPTION

Print every sequence of at least *NUM* printable characters found in each FILE,
one sequence per line.  The default minimum length is 4.

With no FILE, input is read from standard input.  Each FILE is scanned
independently and offsets restart at zero for every file, so several files can
be listed in one invocation.

A byte is *printable* when it is a space, a visible ASCII character (0x20 to
0x7E) or a horizontal tab.  Every other byte — NUL, the remaining control
characters, delete, and any byte with the high bit set — ends the run.  Because
a run never contains a newline, each string occupies exactly one output line.

Output was verified byte for byte against GNU binutils `strings` 2.46 across
every combination of **-f**, **-n**, **-o**, **-t**, **-w** over hand-built
fixtures and over real ELF binaries and shared libraries.  No new dependencies.

# OPTIONS

**-a**, **--all**
:   Scan the entire file, not just the data section.  This is modbox's only
    mode, so this option is accepted and changes nothing (see NOTES).

**-d**, **--data**
:   Only scan the data sections in the file.  Accepted; behaves like **-a**
    (see NOTES).

**-f**, **--print-file-name**
:   Print the name of the file before each string, followed by `: `.

**-n** *NUM*, **--bytes**=*NUM*
:   Print only sequences of at least *NUM* printable characters.  The value may
    be written in decimal, or with a `0x`/`0X` hexadecimal, `0b` binary or
    leading-`0` octal prefix.  *NUM* must be at least 1 and no larger than
    4294967294 (2^32 - 2), mirroring the reference's 32-bit field; a smaller
    value is reported as too small and a larger or negative one as too big.

**-o**
:   An alias for `--radix=o`.  When both **-o** and **-t** are given, **-t**
    wins.

**-s** *STRING*, **--output-separator**=*STRING*
:   Print *STRING* after each string instead of a newline.  Note that this also
    removes the line break, so the output is not one string per line.

**-t** *RADIX*, **--radix**=*RADIX*
:   Print the file offset of each string, where *RADIX* is `o` (octal), `d`
    (decimal) or `x` (hexadecimal).  The offset is that of the string's first
    byte.

**-w**, **--include-all-whitespace**
:   Treat all whitespace as part of a string rather than as a terminator.  The
    whitespace bytes TAB, line feed, vertical tab, form feed, carriage return and
    space are accepted; every other non-printable byte still ends the run.

**-h**, **--help**
:   Display a usage summary and exit.

**-V**, **--version**
:   Output version information and exit.

# OFFSET FORMAT

With **-t** the offset is written in the requested radix and padded on the left
with spaces to a fixed width of 7 columns, followed by one space and the string:

```
      4 Hello
     12 WorldXY
```

The width is constant across radices and across files of any size, so output
stays aligned in a pipeline.

# EXIT STATUS

**modbox strings** exits 0 when every named file was scanned, and 1 when at
least one could not be opened.  A file that fails is reported on standard error
and the remaining files are still scanned, so a mixed invocation both prints the
strings it could read and exits 1.

# EXAMPLES

Print the default four-character strings in a binary:

```
modbox strings /bin/ls
```

Show where each string starts, in hexadecimal, with the file name:

```
modbox strings -f -t x program.elf
```

Require at least ten characters, which cuts most noise out of a large binary:

```
modbox strings -n 10 program.elf
```

Keep whitespace, so an indented block of text stays one string:

```
modbox strings -w -n 8 notes.txt
```

Join the strings with a comma instead of a newline:

```
modbox strings -s ', ' program.elf
```

# NOTES

The following GNU features are **not implemented**:

* **-e** / **--encoding** and **-U** / **--unicode**, which select 16-bit or
  32-bit character units and the UTF-8 display mode.  Only the 7-bit character
  scan is provided.
* **-T** / **--target**, which selects a BFD object format.  This is the one
  place where the output genuinely differs from GNU: modbox scans the whole file
  rather than walking the sections of a recognised object format, so **-d**
  behaves like **-a** and prints a superset of GNU's section data.  On a
  sectioned format such as ELF the extra strings come from the section headers
  and other non-data areas.
* **@** *FILE*, which reads options from a file.

# SEE ALSO

**modbox**(1), **modbox**(1)-**file**(1), **modbox**(1)-**xxd**(1),
**modbox**(1)-**hexdump**(1), **modbox**(1)-**grep**(1)
