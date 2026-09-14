% MODBOX-UNXZ(1) modbox | User Commands
% modbox project
% 2026-09-15

# NAME

modbox-unxz - decompress xz files

# SYNOPSIS

**modbox unxz** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Decompress xz-compressed files. This is equivalent to
`modbox xz -d`, except that decompression is the default action when no
`-d` or `-z` flag is given.

By default each named FILE is replaced by the same name with the `.xz`
suffix removed, and the compressed file is deleted. Use **-k** to keep
it, or **-c** to write the result to standard output instead.

With no FILE, or when FILE is `-`, data is read from standard input and
written to standard output.

# OPTIONS

**-c**, **--stdout**
:   Write to standard output, keeping the original files unchanged.

**-d**, **--decompress**, **--uncompress**
:   Decompress. This is already the default for `unxz`.

**-k**, **--keep**
:   Keep (do not delete) the input files.

**-f**, **--force**
:   Force overwriting of an existing output file. Without this, an
    existing output file is left untouched and an error is reported.

**-0**..**-9**
:   Compression level (only meaningful when compressing via
    **--compress**). `-0` is fastest (least compression), `-9` is
    slowest (best compression). The default is `-6`.

**-q**, **--quiet**
:   Suppress warnings.

**-v**, **--verbose**
:   Print the file name and the compression ratio for each file.

**-h**, **--help**
:   Display help and exit.

**--version**
:   Display version and exit.

# BEHAVIOR NOTES

- Invoking as `unxz` decompresses by default. Passing `-d` is
  redundant but accepted.
- The output is a standard xz stream, interoperable with the system
  `xz`/`unxz` tools.
- Exit code is `0` on success and `1` if any file could not be
  decompressed.

# SEE ALSO

**modbox-xz**(1), **modbox-gunzip**(1), **modbox-bunzip2**(1),
**modbox-zstd**(1).
