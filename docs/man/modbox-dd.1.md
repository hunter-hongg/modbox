% MODBOX-DD(1) modbox | User Commands
% modbox project
% 2026-08-08

# NAME

modbox-dd - convert and copy a file

# SYNOPSIS

**modbox dd** [*OPERAND*]...
**modbox dd** *OPTION*

# DESCRIPTION

Copy a file, converting and formatting according to the operands given.
Operands are specified as `name=value` pairs. When invoked with only an
option (e.g. `--help`), usage is printed.

# OPERANDS

**bs=***BYTES*
:   Read and write up to BYTES bytes at a time (default: 512). Overrides
    `ibs` and `obs`.

**cbs=***BYTES*
:   Convert BYTES bytes at a time (used by `block` and `unblock` conv
    symbols).

**conv=***CONVS*
:   Convert the file as per the comma-separated symbol list. See CONV
    SYMBOLS below.

**count=***N*
:   Copy only N input blocks.

**ibs=***BYTES*
:   Read up to BYTES bytes at a time (default: 512).

**if=***FILE*
:   Read from FILE instead of standard input.

**iflag=***FLAGS*
:   Read using the comma-separated flag list. See FLAG SYMBOLS below.

**obs=***BYTES*
:   Write BYTES bytes at a time (default: 512).

**of=***FILE*
:   Write to FILE instead of standard output.

**oflag=***FLAGS*
:   Write using the comma-separated flag list. See FLAG SYMBOLS below.

**seek=***N*
:   Skip N `obs`-sized output blocks at start of output.

**skip=***N*
:   Skip N `ibs`-sized input blocks at start of input.

**status=***LEVEL*
:   `none` suppresses everything but error messages; `noxfer` suppresses
    the final transfer statistics; `progress` shows periodic transfer
    statistics.

# MULTIPLICATIVE SUFFIXES

N and BYTES may be followed by multiplicative suffixes:

`c`=1, `w`=2, `b`=512, `kB`=1000, `K`=1024, `MB`=1000\*1000,
`M`=1024\*1024, `GB`=1000\*1000\*1000, `G`=1024\*1024\*1024, and so on for
`T`, `P`, `E`, `Z`, `Y`. Binary prefixes: `KiB`=K, `MiB`=M, etc. If N ends
in `B`, it counts bytes not blocks.

# CONV SYMBOLS

`ascii`
:   From EBCDIC to ASCII.

`ebcdic`
:   From ASCII to EBCDIC.

`ibm`
:   From ASCII to alternate EBCDIC.

`block`
:   Pad newline-terminated records with spaces to `cbs` size.

`unblock`
:   Replace trailing spaces in `cbs`-size records with a newline.

`lcase`
:   Change upper case to lower case.

`ucase`
:   Change lower case to upper case.

`swab`
:   Swap every pair of input bytes.

`sync`
:   Pad every input block with NULs to `ibs` size.

`noerror`
:   Continue after read errors.

`notrunc`
:   Do not truncate the output file.

`nocreat`
:   Do not create the output file.

`excl`
:   Fail if the output file already exists.

`fdatasync`
:   Physically write output file data before finishing.

`fsync`
:   Like `fdatasync`, but also write metadata.

# FLAG SYMBOLS

`append`
:   Append mode (makes sense only for output; `conv=notrunc` suggested).

`direct`
:   Use direct I/O for data.

`sync`
:   Use synchronized I/O for data.

`fullblock`
:   Accumulate full blocks of input (iflag only).

`nonblock`
:   Use non-blocking I/O.

`noatime`
:   Do not update access time.

`noctty`
:   Do not assign controlling terminal from file.

`nofollow`
:   Do not follow symbolic links.

`binary`
:   Use binary I/O for data.

`text`
:   Use text I/O for data.

`count_bytes`
:   `count=N` is in bytes.

`skip_bytes`
:   `skip=N` is in bytes.

`seek_bytes`
:   `seek=N` is in bytes.

# EXAMPLES

```bash
# Copy a disk image, 4 KiB blocks, show progress
modbox dd if=/dev/sda of=/backup/sda.img bs=4M status=progress

# Create a 1 MiB sparse-safe file
modbox dd if=/dev/zero of=zeros.bin bs=1M count=1 conv=notrunc

# Convert a file from EBCDIC to ASCII
modbox dd if=input.ebcdic of=output.txt conv=ascii

# Skip the first 1 GiB of input, copy the next 100 MiB
modbox dd if=big.bin of=slice.bin bs=1M skip=1024 count=100
```

# EXIT STATUS

`0`
:   Success.

`1`
:   An error occurred (e.g. invalid operand, permission denied, or read
    error without `conv=noerror`).

# NOTES

- Use `conv=notrunc` when writing into an existing file so the rest of the
  file is preserved.
- With `conv=noerror,sync`, reads that fail are replaced with NULs so the
  copy continues but stays block-aligned.

# SEE ALSO

**modbox-cp**(1), **modbox-tr**(1), **modbox**(1)
