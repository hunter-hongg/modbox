% MODBOX-GUNZIP(1) modbox | User Commands
% modbox project
% 2026-09-15

# NAME

modbox-gunzip - decompress gzip files

# SYNOPSIS

**modbox gunzip** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Decompress gzip-compressed files. This is equivalent to
`modbox gzip -d`, except that decompression is the default action when
no `-d` or `-z` flag is given.

By default each named FILE is replaced by the same name with the `.gz`
suffix removed, and the compressed file is deleted. Use **-k** to keep
it, or **-c** to write the result to standard output instead.

With no FILE, or when FILE is `-`, data is read from standard input and
written to standard output.

# OPTIONS

**-c**, **--stdout**
:   Write to standard output, keeping the original files unchanged.

**-d**, **--decompress**, **--uncompress**
:   Decompress. This is already the default for `gunzip`.

**-k**, **--keep**
:   Keep (do not delete) the input files.

**-f**, **--force**
:   Force overwriting of an existing output file. Without this, an
    existing output file is left untouched and an error is reported.

**-1**..**-9**
:   Compression level (only meaningful when compressing via
    **--compress**). `-1` is fastest (least compression), `-9` is
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

- Invoking as `gunzip` decompresses by default. Passing `-d` is
  redundant but accepted. Passing `-z`/`--compress` reverts to the
  compress mode (upstream `gunzip -z` behaves this way).
- The output is a standard gzip stream, interoperable with the system
  `gzip`/`zcat`/`gunzip` tools.
- Exit code is `0` on success and `1` if any file could not be
  decompressed.

# SEE ALSO

**modbox-gzip**(1), **modbox-bunzip2**(1), **modbox-unxz**(1),
**modbox-zstd**(1).
