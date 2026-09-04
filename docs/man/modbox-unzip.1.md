% MODBOX-UNZIP(1) modbox | User Commands
% modbox project
% 2026-09-02

# NAME

modbox-unzip — list, extract, or test a ZIP archive

# SYNOPSIS

**modbox unzip** [*OPTION*]... *ARCHIVE* [*ENTRY*...]

# DESCRIPTION

Read a ZIP archive and list, extract, or test its contents. The last
positional argument is always the archive path; any positional arguments
before it are entry-name filters (only those entries are acted upon).
When only one positional is given, it is treated as the archive with no
entry filter.

# OPTIONS

**-l**, **--list**
:   List archive contents without extracting. Output includes a header
    row, per-entry uncompressed size, date, time, and name, followed by
    a total line.

**-t**, **--test**
:   Test archive integrity by verifying the CRC32 of every entry.
    Exits non-zero if any entry fails the CRC check.

**-p**, **--stdout**
:   Write a single entry's uncompressed bytes to standard output.
    Requires at least one entry name filter.

**-d**, **--dir=PATH**
:   Extract into the specified directory instead of the current
    directory. Directories are created as needed (equivalent to `mkdir
    -p`).

**-n**, **--no-clobber**
:   Never overwrite existing files. If a target file already exists,
    skip it and continue.

**-o**, **--overwrite**
:   Always overwrite existing files without prompting. This is the only
    mode that replaces existing targets; the default is to skip them.

**-q**, **--quiet**
:   Suppress per-file progress output.

**-v**, **--verbose**
:   Print a verbose header showing the archive path before extraction.

**-h**, **--help**
:   Display usage information and exit.

**--version**
:   Display version and exit.

# BEHAVIOR NOTES

- The default overwrite behavior is to skip existing files silently
  (non-interactive). Use **-o** to force overwrites.
- Directory entries inside the archive are used to recreate the
  directory structure on extraction but are not written as files.
- When entry-name filters are given with **-p**, only the matching entry
  is written to stdout.
- When entry-name filters are given with extraction, only those entries
  are extracted; all others are silently skipped.

# EXAMPLES

```bash
# List all entries in an archive
modbox unzip -l archive.zip

# Test archive integrity
modbox unzip -t archive.zip

# Extract all entries to the current directory
modbox unzip archive.zip

# Extract to a specific directory
modbox unzip -d /tmp/staging archive.zip

# Extract only a specific entry
modbox unzip archive.zip path/to/file.txt

# Print a single entry to stdout for piping
modbox unzip -p archive.zip config.json | jq .

# Extract without overwriting existing files
modbox unzip -n archive.zip

# Force overwrite all existing files
modbox unzip -o archive.zip

# Verbose extraction with progress
modbox unzip -v archive.zip
```

# EXIT STATUS

`0`
:   Operation completed successfully (all entries processed or tested).

non-zero
:   The archive could not be opened, is corrupt, or at least one entry
    failed its integrity check.

# SEE ALSO

**modbox-zip**(1), **modbox-gzip**(1), **modbox-xz**(1), **modbox**(1)
