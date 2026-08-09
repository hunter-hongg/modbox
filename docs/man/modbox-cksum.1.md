% MODBOX-CKSUM(1) modbox | User Commands
% modbox project
% 2026-08-09

# NAME

modbox-cksum - compute CRC checksum and byte count

# SYNOPSIS

**modbox cksum** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Display the CRC (cyclic redundancy check) checksum and byte count of each
FILE.

With no FILE, or when FILE is `-`, read from standard input.

# OPTIONS

**-v**, **--verbose**
:   Output a diagnostic for every file processed (the file name and its
    CRC/byte count are always printed; this option adds per-file detail).

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Print the CRC and byte count of a file
modbox cksum file.txt

# Compute CRCs for several files at once
modbox cksum file1 file2 file3

# Checksum standard input from a pipeline
cat file.txt | modbox cksum

# Verbose mode
modbox cksum -v file.txt
```

# EXIT STATUS

`0`
:   All files processed successfully.

non-zero
:   At least one file could not be read.

# NOTES

- The output format is `CRC byte_count FILE_NAME`, or `CRC byte_count -`
  when reading standard input.
- The CRC algorithm and byte-counting match the POSIX `cksum` utility,
  making the output suitable for cross-checking with other implementations.
- This page documents the current implementation; where it differs from
  GNU coreutils, the modbox behavior is authoritative.

# SEE ALSO

**modbox-sum**(1), **modbox-md5sum**(1), **modbox-cat**(1),
**modbox**(1)
