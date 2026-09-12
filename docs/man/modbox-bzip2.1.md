% MODBOX-BZIP2(1) modbox | User Commands
% modbox project
% 2026-09-12

# NAME

modbox-bzip2 - compress or decompress files with bzip2

# SYNOPSIS

**modbox bzip2** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Compress or decompress FILEs in the bzip2 format. By default each named
FILE is replaced by one with the `.bz2` suffix, and the original file is
removed. Decompression reverses this, replacing `FILE.bz2` with `FILE`
and removing the `.bz2`.

With no FILE, or when FILE is `-`, data is read from standard input and
written to standard output.

The produced container is a standard bzip2 stream (magic `BZh`, followed
by the block-size digit, then a BWT + Huffman coded payload terminated by
a CRC and stream footer) and is interoperable with the system
`bzip2`/`bunzip2`/`bzcat` tools. Concatenated streams are decoded as a
single logical input.

The command is also installed under the names **bunzip2** and **bzcat**,
which select a different default action based on the name used to invoke
it (see *INVOCATION NAMES* below).

# OPTIONS

**-d**, **--decompress**
:   Decompress. This is the default when invoked as `bunzip2` or
    `bzcat`.

**-z**, **--compress**
:   Compress. This is the default when invoked as `bzip2`. It overrides
    a decompression default derived from the invocation name.

**-c**, **--stdout**
:   Write to standard output, keeping the original files unchanged.

**-k**, **--keep**
:   Keep (do not delete) the input files.

**-f**, **--force**
:   Force overwriting of an existing output file. Without this, an
    existing output file is left untouched and an error is reported.

**-t**, **--test**
:   Check the integrity of the compressed input and produce no output.
    This implies **-d**. The input file is not modified or removed.

**-1**..**-9**
:   Compression level, selecting a block size of 100k..900k. `-1` uses
    the least memory and is fastest; `-9` is slowest but compresses
    best. The default is `-9`.

**--fast**
:   Alias for level `-1`.

**--best**
:   Alias for level `-9`.

**-q**, **--quiet**
:   Suppress non-critical error messages, such as the warning that a
    file already has a `.bz2` suffix or that a saved name could not be
    guessed.

**-v**, **--verbose**
:   Print the file name and the compression ratio for each file. With
    **-t**, report `ok` for each verified file.

**-s**, **--small**
:   Accepted for compatibility with the reference implementation and
    ignored; modbox uses a streaming implementation whose memory use is
    bounded independently.

**-h**, **--help**
:   Display help and exit.

**--version**
:   Display version and exit.

# INVOCATION NAMES

The action defaults to compression, except as noted:

**bzip2**
:   Default action is compression.

**bunzip2**
:   Default action is decompression.

**bzcat**
:   Default action is decompression to standard output.

An explicit **-d**/**-z** on the command line overrides the default
derived from the name.

# BEHAVIOR NOTES

- A file whose name already ends in `.bz2` is skipped when compressing
  (with a warning, unless **-q** is given); it is not compressed again.
- When decompressing a file whose name does not end in `.bz2`, the
  original name cannot be recovered from the name, so the output is
  written to `FILE.out` and a warning is printed unless **-q** is given.
- Decompression reports `not a bzip2 file` for input that does not begin
  with a bzip2 stream header, and `data integrity (CRC) error in data`
  when the checksum does not match.
- Input read from a pipe is decoded as a sequence of concatenated
  streams, matching the behavior of the reference implementation.

# EXAMPLES

```bash
# Compress a file in place (file.txt -> file.txt.bz2)
modbox bzip2 file.txt

# Compress keeping the original
modbox bzip2 -k file.txt

# Decompress
modbox bunzip2 file.txt.bz2

# Round-trip through a pipeline
echo "hello" | modbox bzip2 | modbox bunzip2

# Write compressed output to stdout
modbox bzip2 -c file.txt > file.txt.bz2

# Decompress to stdout
modbox bzcat file.txt.bz2

# Verify integrity without producing output
modbox bzip2 -t file.txt.bz2

# Use the fastest level and keep the input
modbox bzip2 -1 -k large.log
```

# EXIT STATUS

`0`
:   All files processed successfully.

non-zero
:   At least one file could not be processed (for example a missing
    input file, corrupt bzip2 data, or a refusal to overwrite an
    existing output file without **-f**).

Note: the reference bzip2 implementation distinguishes warning and
error conditions using the exit codes `1`, `2` and `3`. modbox follows
the convention used by its other compression commands
(**modbox-gzip**(1), **modbox-xz**(1), **modbox-zstd**(1)) and reports
`0` for success and a non-zero code for any failure.

# SEE ALSO

**modbox-bunzip2**(1), **modbox-bzcat**(1), **modbox-gzip**(1),
**modbox-xz**(1), **modbox-zstd**(1), **modbox-tar**(1), **modbox**(1)
