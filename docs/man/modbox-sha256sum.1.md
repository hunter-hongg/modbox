% MODBOX-SHA256SUM(1) modbox | User Commands
% modbox project
% 2026-08-09

# NAME

modbox-sha256sum - compute and check SHA256 (256-bit) checksums

# SYNOPSIS

**modbox sha256sum** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Print or check SHA256 (256-bit) checksums.

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
:   Produce a BSD-style checksum line of the form `SHA256 (file) = hash`.

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
# Compute the SHA256 checksum of a file
modbox sha256sum file.iso

# Compute checksums of several files at once
modbox sha256sum file1 file2 file3 > checksums.sha256

# Verify checksums recorded earlier
modbox sha256sum -c checksums.sha256

# Checksum standard input from a pipeline
cat file.iso | modbox sha256sum

# Verify strictly, reporting only the final status
modbox sha256sum -c --status --strict checksums.sha256
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
  BSD `SHA256 (file) = hash` format.
- When verifying, a file name in the checksum file may contain backslash
  escapes (`\\`, `\n`, `\r`), interpreted as in GNU coreutils.
- This page documents the current implementation; where it differs from
  GNU coreutils, the modbox behavior is authoritative.

# SEE ALSO

**modbox-md5sum**(1), **modbox-b2sum**(1), **modbox-cat**(1),
**modbox**(1)
