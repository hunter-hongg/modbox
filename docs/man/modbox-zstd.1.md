% MODBOX-ZSTD(1) modbox | User Commands
% modbox project
% 2026-08-10

# NAME

modbox-zstd - compress or decompress files with zstd

# SYNOPSIS

**modbox zstd** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Compress or decompress FILEs in the zstd format. By default each named
FILE is replaced by one with the `.zst` suffix, and the original file is
removed. Decompression reverses this, replacing `FILE.zst` with `FILE`
and removing the `.zst`.

With no FILE, or when FILE is `-`, data is read from standard input and
written to standard output.

The produced container is a valid zstd stream (little-endian, 4-byte magic
`28 b5 2f fd`) and is interoperable with the system `zstd` tool.

# OPTIONS

**-c**, **--stdout**
:   Write to standard output, keeping the original files unchanged.

**-d**, **--decompress**
:   Decompress.

**-k**, **--keep**
:   Keep (do not delete) input files.

**-f**, **--force**
:   Force overwrite of output file.

**-1** .. ** -22**
:   Compression level (default 3). Higher levels give better compression
   at the cost of speed.

**-q**, **--quiet**
:   Suppress warnings.

**-v**, **--verbose**
:   Print file name and compression ratio.

**--rm**
:   Remove source file after compression.

**-l**, **--list**
:   List zstd file information.

**-D**, **--dict-id** *ID*
:   Dictionary ID for decompression.

**--no-progress**
:   Disable progress bar.

**-h**, **--help**
:   Display help and exit.

**--version**
:   Display version and exit.

# BEHAVIOR NOTES

- A file whose name already ends in `.zst` is skipped when compressing
  (with a warning, unless **-q** is given); it is not compressed again.
- Decompression always attempts to interpret the input as zstd and
  reports `corrupt data` for input that is not a valid zstd stream.
- The default compression level is 3, matching the zstd CLI default.
- Level flags `-1` through `-22` are supported (both single and multi-digit).

# EXAMPLES

```bash
# Compress a file in place (file.txt -> file.txt.zst)
modbox zstd file.txt

# Compress keeping the original
modbox zstd -k file.txt

# Decompress
modbox zstd -d file.txt.zst

# Round-trip through a pipeline
echo "hello" | modbox zstd | modbox zstd -d

# Write compressed output to stdout
modbox zstd -c file.txt > file.txt.zst

# Decompress to stdout
modbox zstd -dc file.txt.zst
```

# EXIT STATUS

`0`
:   All files processed successfully.

non-zero
:   At least one file could not be processed (for example a missing
    input file, corrupt zstd data, or a refusal to overwrite an
    existing output file without **-f**).

# SEE ALSO

**modbox-gzip**(1), **modbox-xz**(1), **modbox**(1)
