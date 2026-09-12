% MODBOX-ZCAT(1) modbox | User Commands
% modbox project
% 2026-09-11

# NAME

modbox-zcat - decompress gzip files to standard output

# SYNOPSIS

**modbox zcat** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Decompress gzip-compressed files and write the result to standard output.
This is equivalent to `modbox gzip -dc`.

With no *FILE*, or when *FILE* is `-`, read from standard input.

# OPTIONS

**-h**, **--help**
:   Display help and exit.

**--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Decompress a file to the terminal
modbox zcat archive.gz

# Decompress and pipe into another command
modbox zcat archive.gz | grep pattern

# Decompress standard input
cat archive.gz | modbox zcat
```

# EXIT STATUS

`0`
:   All input was decompressed successfully.

non-zero
:   An input file could not be read or was not valid gzip data.

# SEE ALSO

**modbox-gzip**(1), **modbox**(1)
