% MODBOX-GZIP(1) modbox | User Commands
% modbox project
% 2026-08-09

# NAME

modbox-gzip - compress or decompress files with gzip

# SYNOPSIS

**modbox gzip** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Compress or decompress FILEs in the gzip format. By default each named
FILE is replaced by one with the `.gz` suffix, and the original file is
removed. Decompression reverses this, replacing `FILE.gz` with `FILE`
and removing the `.gz`.

With no FILE, or when FILE is `-`, data is read from standard input and
written to standard output.

The produced container is a standard gzip stream (magic `1f 8b`, DEFLATE
payload, CRC-32 and ISIZE trailer) and is interoperable with the system
`gzip`/`zcat`/`gunzip` tools.

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

**-1**..**-9**
:   Compression level: `-1` is fastest (least compression), `-9` is
    slowest (best compression). The default is `-6`.

**--fast**
:   Alias for level `-1`.

**--best**
:   Alias for level `-9`.

**-q**, **--quiet**
:   Suppress warnings (for example the warning that a file already has
    a `.gz` suffix).

**-v**, **--verbose**
:   Print the file name and the compression ratio for each file.

**-h**, **--help**
:   Display help and exit.

**--version**
:   Display version and exit.

# BEHAVIOR NOTES

- A file whose name already ends in `.gz` is skipped when compressing
  (with a warning, unless **-q** is given); it is not compressed again.
- Decompression always attempts to interpret the input as gzip and
  reports `not in gzip format` for input that is not a gzip stream.
- The file name stored in the gzip header is the base name of the
  input (the directory is stripped).
- The modification time stored in the header is taken from the input
  file; for input read from a pipe it is zero.

# EXAMPLES

```bash
# Compress a file in place (file.txt -> file.txt.gz)
modbox gzip file.txt

# Compress keeping the original
modbox gzip -k file.txt

# Decompress
modbox gzip -d file.txt.gz

# Round-trip through a pipeline
echo "hello" | modbox gzip | modbox gzip -d

# Write compressed output to stdout
modbox gzip -c file.txt > file.txt.gz

# Decompress to stdout
modbox gzip -dc file.txt.gz
```

# EXIT STATUS

`0`
:   All files processed successfully.

non-zero
:   At least one file could not be processed (for example a missing
    input file, corrupt gzip data, or a refusal to overwrite an
    existing output file without **-f**).

# SEE ALSO

**modbox-cat**(1), **modbox**(1)
