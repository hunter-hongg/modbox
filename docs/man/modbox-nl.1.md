% MODBOX-NL(1) modbox | User Commands
% modbox project
% 2026-08-11

# NAME

modbox-nl - number lines of files

# SYNOPSIS

**modbox nl** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Number the lines of each FILE and write the result to standard output.
With no FILE, or when FILE is **\-**, read from standard input.

Lines are grouped into header, body, and footer sections using a section
delimiter. By default, only body lines are numbered.

# OPTIONS

**-b**, **--body-numbering=STYLE**
:   Line numbering style for the body section. Default: `a`.

**-d**, **--section-delimiter=CC**
:   Two-character section delimiter. Default: **\\:** (backslash
    followed by colon). Lines containing the delimiter repeated 3 times
    start a header section, 2 times start a body section, and 1 time
    starts a footer section.

**-f**, **--footer-numbering=STYLE**
:   Line numbering style for the footer section. Default: `t`.

**-h**, **--header-numbering=STYLE**
:   Line numbering style for the header section. Default: `t`.

**-i**, **--line-increment=NUMBER**
:   Line number increment. Default: 1.

**-l**, **--join-blank-lines=NUMBER**
:   Group N consecutive blank lines as one for numbering purposes.
    Default: 1 (each blank line is numbered).

**-n**, **--number-format=FORMAT**
:   Number format: `ln` (left-aligned, no leading zeros),
    `rn` (right-aligned, no leading zeros), `rz` (right-aligned,
    zero-padded). Default: `ln`.

**-p**, **--no-renumber**
:   Do not reset line numbers at the start of each section.

**-s**, **--number-separator=STRING**
:   String inserted between the line number and the line content.
    Default: tab.

**-v**, **--starting-line-number=N**
:   Initial line number. Default: 1.

**-w**, **--number-width=NUMBER**
:   Width of the line number field (includes separator). Default: 6.

**-h**, **--help**
:   Display help and exit.

# NUMBERING STYLES

STYLE is one of:

`a`
:   Number all lines (default for body).

`t`
:   Number only non-empty lines (default for header and footer).

`n`
:   Number no lines.

`pBRE`
:   Number only lines matching the basic regular expression BRE.

# SECTION DELIMITER

The default section delimiter is **\\:** (backslash-colon). The number of
consecutive occurrences determines the section type:

3× delimiter (`\:\:`) — header section
2× delimiter (`\:`) — body section
1× delimiter (`:`) — footer section

# EXAMPLES

```bash
# Basic line numbering
modbox nl script.sh

# Zero-padded, right-aligned numbers
modbox nl -n rz -w 4 script.sh

# Skip blank lines in numbering
modbox nl -l 3 data.txt

# Custom line increment
modbox nl -i 5 file.txt

# Custom separator between number and content
modbox nl -s '. ' file.txt

# Start numbering from 10
modbox nl -v 10 file.txt

# Do not renumber at section boundaries
modbox nl -p file.txt
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- The **\-h** short option is `--help`; there is no separate short
  option for `--header-numbering` (it is `-h` only when not confused
  with help — use `--header-numbering` to be explicit).
- Blank lines are numbered by default (`-l 1`); use **-l** with a
  larger value to group consecutive blank lines.
- **--no-renumber** (-p) preserves line numbers across section
  boundaries, useful for documents with multiple body sections.

# SEE ALSO

**modbox-cat**(1), **modbox-nawk**(1), **modbox-awk**(1), **modbox**(1)
