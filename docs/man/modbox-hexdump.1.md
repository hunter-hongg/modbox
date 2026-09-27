% MODBOX-HEXDUMP(1) modbox | User Commands
% modbox project
% 2026-09-15

# NAME

modbox-hexdump - display file contents in hex, decimal, octal and ASCII

# SYNOPSIS

**modbox hexdump** [*OPTION*]... [FILE...]

# DESCRIPTION

Display the contents of each FILE in a numbered and formatted form.

With no FILE, or when FILE is `-`, input is read from standard input.

All files form a single continuous stream: they are concatenated before
display, so the offsets run on across a file boundary rather than restarting.
**-s** skips into that concatenation and **-n** bounds the total number of
bytes read from it.

By default bytes are shown as hexadecimal in two-byte groups, with the pairs
printed in the host's native byte order.

A format option selects how the data column is rendered.  Several may be given,
and the formats *interleave*: each data block is printed once per requested
format, in the order the options appear, before the next block begins.  So
`-C -x` prints a canonical line followed by its two-byte hexadecimal line, then
the next block.  Naming a format twice makes it print every block rather than
being compared against its own first occurrence.  The final offset line is
written once, after every block, and uses the width of the last format.

# OPTIONS

**-b**
:   One-byte octal display.

**-C**, **--canonical**
:   Canonical hexadecimal plus an ASCII gutter.  Bytes that are not printable
    appear as `.` in the gutter.  A short final line keeps the hexadecimal field
    full width but trims the gutter to the bytes that were read.

**-c**, **--one-byte-char**
:   One-byte character display.  A printable byte stands for itself; a zero byte
    becomes `\0` and the other control characters with a backslash form become
    `\a`, `\b`, `\f`, `\n`, `\r`, `\t` or `\v`.  Every other non-printable byte
    is shown as a three-digit octal number.

**-d**, **--two-bytes-decimal**
:   Two-byte decimal display.

**-h**, **--help**
:   Display help and exit.

**-n** *LEN*, **--length**=*LEN*
:   Interpret only the first *LEN* bytes of input.  *LEN* may be given in
    decimal, in hexadecimal with a `0x` prefix, or with a `KiB`, `MiB` or `GiB`
    suffix.

**-o**, **--two-bytes-octal**
:   Two-byte octal display.

**-s** *OFFSET*, **--skip**=*OFFSET*
:   Skip *OFFSET* bytes at the start of the stream.  The offset is absolute, so
    the first line printed is still numbered from *OFFSET*.  *OFFSET* accepts the
    same decimal, hexadecimal and suffix forms as **-n**.  Skipping to or past
    the end of the input prints the closing offset alone, with no data lines.

**-v**, **--no-squeezing**
:   Display all input data.  Without this option a run of identical blocks is
    replaced by a single `*`, and a run of several repeated blocks still yields
    one `*`.  The comparison covers the body of a line, not its offset, and is
    tracked per format option.

**-V**, **--version**
:   Output version information and exit.

**-x**, **--two-bytes-hex**
:   Two-byte hexadecimal display.  This is the default.

**-X**, **--one-byte-hex**
:   One-byte hexadecimal display.

# EXAMPLES

```bash
# Dump a file in the default two-byte hexadecimal layout
modbox hexdump payload.bin

# Pipe through stdin and stdout
cat saved.bin | modbox hexdump

# Canonical hex with an ASCII gutter
modbox hexdump -C payload.bin

# Interleaved: a canonical line, then the one-byte hex line for the same block
modbox hexdump -C -X payload.bin

# Start 256 bytes in, and stop after 512 bytes
modbox hexdump -s 0x100 -n 512 payload.bin

# Concatenate two files into one continuous offset run
modbox hexdump header.bin body.bin

# Show the character column, keeping every repeated line
modbox hexdump -c -v payload.bin
```

# EXIT STATUS

`0`
:   Success.

`1`
:   An error: an unknown option, an invalid **-s** or **-n** value, a file that
    could not be opened, or standard input that could not be read.

# NOTES

- The default format and the two-byte formats print each pair of bytes in the
  host's native order, which is little endian on every platform modbox targets.
  The one-byte formats and **-C** are unaffected, since they never group bytes.
- The reference prints an `All input file arguments failed` summary line when
  no file could be opened; this command reports each failure on its own and
  does not add the summary.
- The error wording follows the convention used by the other modbox commands
  rather than the reference's translated strings, so a missing file reports
  `No such file or directory` where the reference uses the locale's equivalent.
- Every exit status for an error is `1`, matching the reference, including
  usage errors.
- An input that holds no bytes produces no output at all, even with a `-s`
  that would skip past the end.  An input that does hold bytes but yields
  nothing after `-s` prints the closing offset on its own.

# SEE ALSO

**modbox-od**(1), **modbox-xxd**(1), **modbox**(1)
