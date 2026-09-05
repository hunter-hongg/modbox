% MODBOX-TAR(1) modbox | User Commands
% modbox project
% 2026-09-05

# NAME

modbox-tar — create, extract, or list tar archives

# SYNOPSIS

**modbox tar** [*OPTION*]... { -c | -x | -t } [*ARCHIVE*] [*FILE*]...

# DESCRIPTION

Create, extract, or list POSIX tar archives with optional gzip/xz/zstd
compression. Supported formats are ustar, GNU longname (L/K), and pax
extension headers. The command follows GNU tar conventions for short and
long options, compression auto-detection, and pattern filtering.

# OPTIONS

**-c**, **--create**
:   Create a new archive.

**-x**, **--extract**
:   Extract files from an archive.

**-t**, **--list**
:   List archive contents.

**-f**, **--file=ARCHIVE**
:   Use archive file ARCHIVE. '-' means stdin/stdout.

**-C**, **--directory=DIR**
:   Change to DIR before operating.

**-z**, **--gzip**
:   Filter the archive with gzip.

**-j**, **--bzip2**
:   Filter the archive with xz (liblzma).

**-J**, **--zstd**
:   Filter the archive with zstd.

**-p**, **--preserve-permissions**
:   Extract with full permission sets.

**-o**, **--no-same-owner**
:   Extract as current user.

**--format=FORMAT**
:   Archive format: gnu (default) or pax.

**-h**, **--help**
:   Display usage information and exit.

**--version**
:   Display version and exit.

# EXAMPLES

```bash
# Create archive
modbox tar -c -f out.tar files...

# List archive
modbox tar -t -f out.tar

# Extract
modbox tar -x -f out.tar
```

# SEE ALSO

modbox-zip(1), modbox-unzip(1)
