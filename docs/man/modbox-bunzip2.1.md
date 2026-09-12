% MODBOX-BUNZIP2(1) modbox | User Commands
% modbox project
% 2026-09-12

# NAME

modbox-bunzip2 - decompress bzip2 files

# SYNOPSIS

**modbox bunzip2** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Decompress bzip2-compressed files. This is equivalent to
`modbox bzip2 -d`.

By default each named FILE is replaced by the same name with the `.bz2`
suffix removed, and the compressed file is deleted. Use **-k** to keep
it, or **-c** to write the result to standard output instead.

With no FILE, or when FILE is `-`, data is read from standard input and
written to standard output.

Concatenated bzip2 streams are decoded as a single logical input.

# OPTIONS

**-c**, **--stdout**
:   Write to standard output, keeping the original files unchanged.

**-k**, **--keep**
:   Keep (do not delete) the input files.

**-f**, **--force**
:   Force overwriting of an existing output file. Without this, an
    existing output file is left untouched and an error is reported.

**-t**, **--test**
:   Check the integrity of the compressed input and produce no output.
    The input file is not modified or removed.

**-d**, **--decompress**
:   Decompress. This is already the default for `bunzip2`.

**-z**, **--compress**
:   Compress instead of decompressing.

**-q**, **--quiet**
:   Suppress non-critical error messages, such as the warning that a
    saved name could not be guessed.

**-v**, **--verbose**
:   Print the file name and the compression ratio for each file. With
    **-t**, report `ok` for each verified file.

**-h**, **--help**
:   Display help and exit.

**--version**
:   Display version and exit.

# BEHAVIOR NOTES

- When the input name does not end in `.bz2`, the original name cannot
  be recovered, so the output is written to `FILE.out` and a warning is
  printed unless **-q** is given.
- Input that does not begin with a bzip2 stream header is reported as
  `not a bzip2 file`; a checksum mismatch is reported as
  `data integrity (CRC) error in data`.
- The compression level options **-1**..**-9** and **-s** are accepted
  and ignored, since they only affect compression.

# EXAMPLES

```bash
# Decompress a file in place (file.txt.bz2 -> file.txt)
modbox bunzip2 file.txt.bz2

# Decompress keeping the compressed file
modbox bunzip2 -k file.txt.bz2

# Decompress to standard output
modbox bunzip2 -c file.txt.bz2

# Decompress standard input
cat file.txt.bz2 | modbox bunzip2

# Verify integrity without producing output
modbox bunzip2 -t file.txt.bz2
```

# EXIT STATUS

`0`
:   All input was decompressed successfully.

non-zero
:   An input file could not be read or was not valid bzip2 data.

Note: the reference bzip2 implementation distinguishes warning and
error conditions using the exit codes `1`, `2` and `3`. modbox follows
the convention used by its other compression commands
(**modbox-gzip**(1), **modbox-xz**(1), **modbox-zstd**(1)) and reports
`0` for success and a non-zero code for any failure.

# SEE ALSO

**modbox-bzip2**(1), **modbox-bzcat**(1), **modbox-gzip**(1),
**modbox-xz**(1), **modbox-zstd**(1), **modbox-tar**(1), **modbox**(1)
