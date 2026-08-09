% MODBOX-MD5SUM(1) modbox | User Commands
% modbox project
% 2026-08-09

# NAME

modbox-md5sum - compute and check MD5 (128-bit) checksums

# SYNOPSIS

**modbox md5sum** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Print or check MD5 (128-bit) checksums.

With no FILE, or when FILE is `-`, read from standard input.

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
:   Produce a BSD-style checksum line of the form `MD5 (file) = hash`.

**-z**, **--zero**
:   End each output line with a NUL character instead of a newline.

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
# Compute the MD5 checksum of a file
modbox md5sum file.iso

# Compute checksums of several files at once
modbox md5sum file1 file2 file3 > checksums.md5

# Verify checksums recorded earlier
modbox md5sum -c checksums.md5

# Checksum standard input from a pipeline
cat file.iso | modbox md5sum

# Verify strictly, reporting only the final status
modbox md5sum -c --status --strict checksums.md5
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

- modbox computes digests with OpenSSL and prints them as lowercase
  hexadecimal.
- Output lines use the format `hash`, then a space (text mode) or asterisk
  (binary mode), then the file name. The **--tag** option switches to the
  BSD `MD5 (file) = hash` format.
- When verifying, a file name in the checksum file may contain backslash
  escapes (`\\`, `\n`, `\r`), interpreted as in GNU coreutils.
- This page documents the current implementation; where it differs from
  GNU coreutils, the modbox behavior is authoritative.

# SEE ALSO

**modbox-sha256sum**(1), **modbox-b2sum**(1), **modbox-cat**(1),
**modbox**(1)
