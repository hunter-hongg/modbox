% MODBOX-B2SUM(1) modbox | User Commands
% modbox project
% 2026-08-09

# NAME

modbox-b2sum - compute and check BLAKE2 (512-bit) checksums

# SYNOPSIS

**modbox b2sum** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Print or check BLAKE2 (512-bit) checksums.

With no FILE, or when FILE is `-`, read from standard input.

Unlike the fixed-width MD5/SHA family, BLAKE2 supports variable-length
digests; the **-l**, **--length** option selects the number of bits.

# OPTIONS

**-b**, **--binary**
:   Read in binary mode. The output line marks binary mode with an
    asterisk (`*`) before the file name.

**-t**, **--text**
:   Read in text mode (default). The output line uses a space before the
    file name.

**-c**, **--check**
:   Read checksums from the FILEs and verify them against the named
    files. Report `OK` or `FAILED` for each, and exit non-zero on any
    mismatch, missing file, or malformed line (subject to the verification
    options below).

**--tag**
:   Produce a BSD-style checksum line of the form `BLAKE2 (file) = hash`.

**-z**, **--zero**
:   End each output line with a NUL character instead of a newline.

**-l**, **--length**=*BITS*
:   Digest length in bits (default 512). Must be between 8 and 512,
    inclusive. The printed digest is truncated to this length, so its
    width in hexadecimal characters is half the bit count.

The following five options are useful only when verifying checksums with
**-c**:

**--ignore-missing**
:   Do not fail or report status for files listed in the checksum file
    that cannot be read.

**--quiet**
:   Do not print `OK` for each successfully verified file.

**--status**
:   Do not output anything; the exit status indicates success or failure.

**--strict**
:   Exit non-zero for improperly formatted checksum lines.

**-w**, **--warn**
:   Warn about improperly formatted checksum lines on standard error.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Compute the default (512-bit) BLAKE2 checksum
modbox b2sum file.iso

# Compute a 256-bit BLAKE2 checksum
modbox b2sum -l 256 file.iso

# Compute checksums of several files at once
modbox b2sum file1 file2 file3 > checksums.b2

# Verify checksums recorded earlier
modbox b2sum -c checksums.b2

# Verify strictly, reporting only the final status
modbox b2sum -c --status --strict checksums.b2
```

# EXIT STATUS

`0`
:   All checksums computed, or (in check mode) every listed file matched
    and was readable.

non-zero
:   In check mode, at least one computed checksum did not match, a listed
    file could not be read, or a checksum line was improperly formatted
    (with **--strict**); otherwise, at least one input file could not be
    read.

# NOTES

- modbox computes digests with OpenSSL (BLAKE2b-512) and prints them as
  lowercase hexadecimal.
- The digest width is variable: a 256-bit digest prints as 64 hex
  characters, a 512-bit digest as 128. When verifying, the expected
  digest length is inferred from the number of hex characters on each
  checksum line.
- Output lines use the format `hash`, then a space (text mode) or asterisk
  (binary mode), then the file name. The **--tag** option switches to the
  BSD `BLAKE2 (file) = hash` format.
- This page documents the current implementation; where it differs from
  GNU coreutils, the modbox behavior is authoritative.

# SEE ALSO

**modbox-md5sum**(1), **modbox-sha256sum**(1), **modbox-cat**(1),
**modbox**(1)
