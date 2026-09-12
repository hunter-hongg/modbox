% MODBOX-CMP(1) modbox | User Commands
% modbox project
% 2026-08-12

# NAME

modbox-cmp - compare two files byte by byte

# SYNOPSIS

**modbox cmp** [*OPTION*]... FILE1 [FILE2 [SKIP1 [SKIP2]]]

# DESCRIPTION

Compare two files byte by byte.  FILE1 and FILE2 are compared starting
at byte zero; the first byte that differs is reported on standard
output.  Unlike **modbox-diff**(1), which compares line by line, `cmp`
is binary-safe: NUL bytes and non-printable data are handled correctly,
making it suitable for comparing arbitrary files.

With no FILE2, or when a FILE is `-`, input is read from standard
input.  At most one of FILE1 and FILE2 may be `-`.

The optional SKIP1 and SKIP2 operands are a shorthand for
**-i** *SKIP1*:*SKIP2* (see below).

# OPTIONS

**-b**, **--print-bytes**
:   Print the differing bytes in octal and as a printable mnemonic
    alongside the default "differ" message.  Use this together with
    **-l** to annotate every listed difference.

**-i** *SKIP*, **--ignore-initial**=*SKIP*
:   Skip the first *SKIP* bytes of both inputs.  With a colon-separated
    *SKIP1*:*SKIP2* argument, skip *SKIP1* bytes of FILE1 and *SKIP2*
    bytes of FILE2 instead.  Skipped bytes do not participate in the
    comparison; differences past the skip point are still reported.

**-l**, **--verbose**
:   Output the byte number (starting at 1) and the octal values of each
    differing byte pair, one difference per line, for all differences.
    This option is mutually exclusive with **-s**.

**-n** *LIMIT*, **--bytes**=*LIMIT*
:   Compare at most *LIMIT* bytes.  With *LIMIT* zero, nothing is read
    and the inputs are reported as equal.

**-s**, **--quiet**, **--silent**
:   Suppress all normal output.  Only the exit status is meaningful.
    This option is mutually exclusive with **-l**.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Compare two files; report the first difference
modbox cmp old.bin new.bin

# Report every differing byte with its octal value
modbox cmp -l old.bin new.bin

# Annotate differences with printable mnemonics
modbox cmp -lb old.bin new.bin

# Quietly test whether two files are identical (for scripts)
modbox cmp -s a.txt a.txt && echo identical

# Compare only the first 512 bytes, ignoring an 8-byte header
modbox cmp -n 512 -i 8 disk1.img disk2.img

# Use the SKIP1:SKIP2 shorthand as positional operands
modbox cmp a.bin b.bin 8 16

# Compare a file against standard input
cat saved.txt | modbox cmp saved.txt -
```

# EXIT STATUS

`0`
:   The inputs are identical.

`1`
:   The inputs differ, or one input is a prefix of the other (including
    the case where one input is empty).

`2`
:   An error occurred (e.g., a file could not be opened, an option was
    invalid, or **-l** and **-s** were used together).

# NOTES

- `cmp` reads and compares raw bytes and never interprets the content as
  text, so it works on binaries, archives, and other non-text data.
- When one input is shorter than the other, a diagnostic is written to
  standard error naming the shorter input together with the number of
  bytes (and lines) read before end-of-file.
- For a human-readable, line-oriented comparison that produces a diff,
  use **modbox-diff**(1) instead.

# SEE ALSO

**modbox-diff**(1), **modbox-diff3**(1), **modbox-comm**(1), **modbox**(1)
