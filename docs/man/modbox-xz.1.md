% MODBOX-XZ(1) modbox | User Commands
% modbox project
% 2026-08-11

# NAME

modbox-xz - compress or decompress files with xz

# SYNOPSIS

**modbox xz** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Compress or decompress FILEs in the xz format. By default each named
FILE is replaced by one with the `.xz` suffix, and the original file is
removed. Decompression reverses this, replacing `FILE.xz` with `FILE`
and removing the `.xz`.

With no FILE, or when FILE is `-`, data is read from standard input and
written to standard output.

The produced container is a valid xz stream (little-endian, 4-byte magic
`fd 37 7a 58 5a 00`) and is interoperable with the system `xz`/`lzma`
tools.

# OPTIONS

**-c**, **--stdout**
:   Write to standard output, keeping the original files unchanged.

**-d**, **--decompress**, **--uncompress**
:   Decompress.

**-k**, **--keep**
:   Keep (do not delete) the input files.

**-f**, **--force**
:   Force overwriting of an existing output file. Without this, an
    existing output file is left untouched and an error is reported.

**-0**..**-9**
:   Compression level: `-0` is fastest (least compression), `-9` is
    slowest (best compression). The default is `-6`.

**-q**, **--quiet**
:   Suppress warnings (for example the warning that a file already has
    a `.xz` suffix).

**-v**, **--verbose**
:   Print the file name and the compression ratio for each file.

**-h**, **--help**
:   Display help and exit.

**--version**
:   Display version and exit.

# BEHAVIOR NOTES

- A file whose name already ends in `.xz` is skipped when compressing
  (with a warning, unless **-q** is given); it is not compressed again.
- Decompression always attempts to interpret the input as xz and
  reports `corrupt data` for input that is not a valid xz stream.
- The default check type is CRC64, matching `xz` CLI default.
- The trade-off is less content over header fields (no custom mtime
  embedding) but those are not part of the GNU xz contract anyway.

# EXAMPLES

```bash
# Compress a file in place (file.txt -> file.txt.xz)
modbox xz file.txt

# Compress keeping the original
modbox xz -k file.txt

# Decompress
modbox xz -d file.txt.xz

# Round-trip through a pipeline
echo "hello" | modbox xz | modbox xz -d

# Write compressed output to stdout
modbox xz -c file.txt > file.txt.xz

# Decompress to stdout
modbox xz -dc file.txt.xz
```

# EXIT STATUS

`0`
:   All files processed successfully.

non-zero
:   At least one file could not be processed (for example a missing
    input file, corrupt xz data, or a refusal to overwrite an
    existing output file without **-f**).

# SEE ALSO

**modbox-gzip**(1), **modbox-cat**(1), **modbox**(1)
