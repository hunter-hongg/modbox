% MODBOX-XXD(1) modbox | User Commands
% modbox project
% 2026-09-15

# NAME

modbox-xxd - make a hexdump or do the reverse

# SYNOPSIS

**modbox xxd** [*OPTION*]... [FILE [OUTFILE]]

**modbox xxd** **-r** [*OPTION*]... [FILE [OUTFILE]]

# DESCRIPTION

Produce a hexadecimal, postscript plain, or C include style dump of FILE, or
with **-r** convert a hexdump back to binary.

With no FILE, or when FILE is `-`, input is read from standard input.
By default the output goes to standard output; an OUTFILE sends the output
there instead.

When FILE is a directory, GNU `xxd` reads it; this implementation follows
the usual file-open semantics and reports an error instead.

# OPTIONS

**-a**, **--autoskip**
:   Replace runs of eight bytes of zero (a whole line that is all NUL) with a
    single `*` instead of printing each line.  Default is off.

**-b**, **--binary**
:   Dump octets in binary (eight 0/1 digits per byte) rather than hex.  Cannot
    be combined with **-ps**.

**-C**, **--capitalize**
:   Uppercase the variable name produced by **-i**.  This also uppercases the
    `_len` suffix, which becomes `_LEN`.

**-c** *COLS*, **--cols**=*COLS*
:   Format *COLS* octets per line.  The default is 16, but 12 for **-i** and
    30 for **-ps**.

**-d**, **--decimal**
:   Show the offset in decimal instead of hexadecimal.

**-E**, **--ebcdic**
:   Show characters in the right-hand gutter using EBCDIC transliteration
    instead of ASCII.  Bytes without a printable equivalent render as `.`.

**-e**, **--little-endian**
:   Reverse the bytes inside each group before printing, giving a
    little-endian view.  Incompatible with **-ps**, **-i** and **-r**, and with
    a group size that is not a power of two.

**-g** *BYTES*, **--groupsize**=*BYTES*
:   Number of octets per group in normal output.  Default 2, or 4 with **-e**.

**-h**, **--help**
:   Display help and exit.

**-i**, **--include**
:   Output in C include file style: an `unsigned char` array plus a length
    variable.  See the VARIABLE NAMES note below.

**-l** *LEN*, **--length**=*LEN*
:   Stop after *LEN* octets.

**-n** *NAME*, **--name**=*NAME*
:   Use *NAME* as the variable name in **-i** output instead of deriving one
    from the input path.

**-o** *OFF*, **--offset**=*OFF*
:   Add *OFF* to the displayed file position.

**-ps**, **--postscript**
:   Output in postscript plain hexdump style: just hex bytes, no offsets and
    no gutter, 30 per line by default.  Cannot be combined with **-b** or
    with **-e**.

**-r**, **--reverse**
:   Reverse operation: convert (or patch) a hexdump into binary.

**-r** **-s** *OFF*
:   When reversing, add *OFF* to the file positions found in the hexdump.

**-R** *WHEN*
:   Colourise the output.  *WHEN* is `always`, `auto` or `never`, with `auto`
    being the default.  This is accepted for compatibility but does nothing.

**-s** [*+*][*-*]*SEEK*, **--seek**=*SEEK*
:   Start at byte *SEEK*, either as an absolute offset or, with a leading `+`,
    as a relative one.  Seeking backwards in reverse mode is rejected.

**-t**, **--terminate**
:   Append a terminating zero to the **-i** output.

**-u**, **--upper**
:   Use uppercase hex letters.

**-V**, **--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Dump a file in the default 16-bytes-per-line layout
modbox xxd payload.bin

# Pipe through stdin and stdout
cat saved.bin | modbox xxd

# Fewer columns per line, grouped two at a time
modbox xxd -c 8 -g 2 payload.bin

# Plain hex for a shell pipeline (no offsets, no gutter)
modbox xxd -ps payload.bin

# Reverse a plain hexdump back into binary
modbox xxd -r -ps < dump.hex > payload.bin

# Emit a C array header from a file, with a chosen name
modbox xxd -i -n config_bytes config.dat

# Start at a fixed offset and show uppercase hex
modbox xxd -s 0x100 -u payload.bin
```

# EXIT STATUS

`0`
:   Success.

`1`
:   A usage error: an invalid option, an incompatible combination of options
    (such as **-b** with **-ps**, or **-e** with **-i**), or a rejected seek.

`2`
:   An I/O error, such as a file that could not be opened for reading or an
    output file that could not be written.

# NOTES

- The variable name in **-i** output is derived from the file *basename*
  rather than the whole path, so `/tmp/foo.dat` yields `foo_dat` where the
  reference `xxd` would emit `_tmp_foo_dat`.  Every character that is not
  alphanumeric and is not an underscore becomes `_`.  This can yield a name
  that begins with a digit, as in `9lead.dat` becoming `9lead_dat`, which is
  not a valid C identifier; the reference tool prefixes such names with an
  underscore.  Relative paths and explicit **-n** names behave as upstream.

- Reading **-i** input from standard input always produces a complete,
  wrapped declaration using the name `stdin`, including the trailing
  `stdin_len` variable.  The reference `xxd` omits the `unsigned char`
  header, the closing `};` and the length variable in this case, leaving
  bare hex lines, since no name can be derived from `-`.

- The reference `xxd` prints its help text to standard error and exits `1`
  for **-h**; this command prints help to standard output and exits `0`,
  matching the convention used by the other modbox commands.  The version
  string is likewise `xxd (modbox) 1.0` rather than the upstream
  date-and-author form.

- **-R** is parsed and ignored.  Hex output is written as plain text in every
  case, so no colour is ever emitted regardless of the requested setting.

- A few **-r** edge cases differ from upstream.  Without **-ps**, GNU `xxd`
  only parses lines in full hexdump format, so bare hex such as
  `41424344` restores nothing; this command treats bare hex as valid input
  and restores it.  A positive **-r -s** offset also behaves differently:
  GNU pads the gap with NUL bytes (`-r -s 256` on a 4-byte dump yields 260
  bytes), whereas this command emits only the decoded bytes.  Normal
  `-r -ps` round-tripping matches byte for byte.  For **-e -r**, GNU prints
  `Sorry, cannot revert this type of hexdump` and exits `255`, while this
  command prints the usage summary and exits `1`.

- Option-combination handling differs slightly from upstream.  Both reject
  **-b** with **-ps** and **-e** with **-ps**/**-i**, but this command prints
  the usage summary (GNU prints `only one of -b, -e, -u, -p, -i can be
  used`).  Two combinations are *more* permissive here: **-b** with **-e** is
  accepted (GNU rejects it), and **-b -i** emits a hexadecimal array rather
  than GNU's `0b...` binary-format array.  **-u -i** emits `0x` prefixes
  rather than GNU's `0X`.  An **-e** group size that is not a power of two is
  rejected with a message matching the reference tool's exactly.

# SEE ALSO

**modbox-od**(1), **modbox-tcpdump**(1), **modbox**(1)
