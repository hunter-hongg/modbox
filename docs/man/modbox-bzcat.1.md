% MODBOX-BZCAT(1) modbox | User Commands
% modbox project
% 2026-09-12

# NAME

modbox-bzcat - decompress bzip2 files to standard output

# SYNOPSIS

**modbox bzcat** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Decompress bzip2-compressed files and write the result to standard
output. This is equivalent to `modbox bzip2 -dc`.

The compressed input files are left unchanged.

With no *FILE*, or when *FILE* is `-`, read from standard input.

Concatenated bzip2 streams are decoded as a single logical input.

# OPTIONS

**-c**, **--stdout**
:   Write to standard output. This is already the default for `bzcat`.

**-k**, **--keep**
:   Keep (do not delete) the input files. This is already the default
    for `bzcat`.

**-t**, **--test**
:   Check the integrity of the compressed input and produce no output.

**-d**, **--decompress**
:   Decompress. This is already the default for `bzcat`.

**-z**, **--compress**
:   Compress instead of decompressing.

**-q**, **--quiet**
:   Suppress non-critical error messages.

**-v**, **--verbose**
:   Print the file name and the compression ratio for each file.

**-h**, **--help**
:   Display help and exit.

**--version**
:   Display version and exit.

# BEHAVIOR NOTES

- Because output always goes to standard output, no output file is
  created and no input file is removed, regardless of **-k**.
- Input that does not begin with a bzip2 stream header is reported as
  `not a bzip2 file`; a checksum mismatch is reported as
  `data integrity (CRC) error in data`.

# EXAMPLES

```bash
# Decompress a file to the terminal
modbox bzcat archive.txt.bz2

# Decompress and pipe into another command
modbox bzcat archive.txt.bz2 | grep pattern

# Decompress standard input
cat archive.txt.bz2 | modbox bzcat
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

**modbox-bzip2**(1), **modbox-bunzip2**(1), **modbox-zcat**(1),
**modbox-gzip**(1), **modbox-xz**(1), **modbox-zstd**(1), **modbox**(1)
