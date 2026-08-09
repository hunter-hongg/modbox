% MODBOX-SUM(1) modbox | User Commands
% modbox project
% 2026-08-09

# NAME

modbox-sum - compute BSD- and System V-style checksums


# SYNOPSIS

**modbox sum** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Print the checksum and block count for each FILE.

With no FILE, or when FILE is `-`, read from standard input.

Two historical checksum formats are supported: System V (the default) and
BSD.

# OPTIONS

**--sysv**
:   Use the System V `sum` format (default). The checksum is taken modulo
    65536.

**--bsd**
:   Use the BSD `sum` format: a 32-bit checksum.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Print System V checksums (default)
modbox sum file1 file2 file3

# Print BSD checksums
modbox sum --bsd file1 file2 file3

# Checksum standard input from a pipeline
cat file.txt | modbox sum

# Explicitly request the System V format
modbox sum --sysv file.txt
```

# EXIT STATUS

`0`
:   All files processed successfully.

non-zero
:   Currently modbox returns 0 in all cases — even when an input file
    cannot be read. (GNU coreutils sum returns non-zero in that case; this
    is a known modbox deviation.)


# NOTES

- The output format is `CHECKSUM BLOCKS FILE`, where `BLOCKS` is the file
  size in 512-byte blocks (rounded up).
- `--sysv` is the default format; `--bsd` selects the 32-bit BSD variant.
- **`--bsd` deviation:** modbox's BSD format uses 512-byte blocks for the
  block count (e.g., 4097 bytes reports 9 blocks) and a modbox-specific
  checksum, so its output differs from BSD/GNU `sum` for files larger than
  4096 bytes.

- **Implementation note:** the command's `--help` text currently advertises
  short options `-s`/`--sysv` and `-r`/`--bsd`, but only the long forms
  `--sysv` and `--bsd` are actually accepted. Use the long forms. This
  discrepancy is tracked for a future fix and does not affect the documented
  behavior.
- This page documents the current implementation; where it differs from
  GNU coreutils, the modbox behavior is authoritative.

# SEE ALSO

**modbox-cksum**(1), **modbox-md5sum**(1), **modbox-cat**(1),
**modbox**(1)
