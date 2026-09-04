% MODBOX-ZIP(1) modbox | User Commands
% modbox project
% 2026-09-02

# NAME

modbox-zip — create or update a ZIP archive

# SYNOPSIS

**modbox zip** [*OPTION*]... *ARCHIVE* [*FILE*]...

# DESCRIPTION

Add or update members of a ZIP archive. The first positional argument is
always the archive path; all remaining positional arguments are input
files or directories.

With no input paths and no **-@**, the command exits with an error. To
add a single entry from stdin, use **-** as the sole input path or use
**-@** to read names from stdin one per line.

# OPTIONS

**-r**, **--recursive**
:   Recurse into directories; every regular file, subdirectory, and
    symlink under the path is added to the archive.

**-j**, **--junk-paths**
:   Store entries using only the base name, stripping all directory
    components. A file at *dir/foo.txt* is stored as *foo.txt*.

**-u**, **--update**
:   Update existing entries only when the source file is newer than the
    archive member. When the archive does not yet exist, this is an
    error.

**-f**, **--freshen**
:   Re-compress existing entries only when the source file is newer.
    Entries not referenced by the user are preserved. When the archive
    does not yet exist, this is an error.

**-d**, **--delete**
:   Delete named entries from an existing archive. The archive is
    rewritten without the matching members.

**-x**, **--exclude=GLOB**
:   Exclude entries whose internal name matches the given glob pattern.
    '*' matches any sequence of characters (except '/'), '?' matches any
    single character.

**-@**, **--stdin-names**
:   Read input file names from standard input, one per line. Blank lines
    and lines containing only whitespace are skipped.

**-1** .. ** -9**
:   Deflate compression level. Level 1 is fastest, level 9 gives the
    best compression. Default is 6.

**--fast**
:   Alias for level 1.

**--best**
:   Alias for level 9.

**-q**, **--quiet**
:   Suppress per-file progress output.

**-v**, **--verbose**
:   Print a verbose line for each entry, including compressed size and
    ratio.

**-h**, **--help**
:   Display usage information and exit.

**--version**
:   Display version and exit.

# BEHAVIOR NOTES

- Leading `./` and leading `/` are stripped from entry names so the
  archive contents use clean relative paths.
- Empty files are stored correctly and extract as zero-length files.
- Symlinks inside a directory tree are stored as symlink entries; their
  targets are preserved but v1 restores them as regular files on
  extraction.
- `-u` and `-f` are mutually exclusive with each other and with `-d`;
  specifying more than one of these flags at a time is an error.

# EXAMPLES

```bash
# Bundle two files into an archive
modbox zip archive.zip file1.txt file2.txt

# Recursively archive a directory
modbox zip -r archive.zip myproject/

# Flatten all paths into a single level
modbox zip -j flat.zip myproject/

# Update only newer files in an existing archive
modbox zip -u archive.zip newfile.txt

# Delete an entry from an archive
modbox zip -d archive.zip oldfile.txt

# Exclude log files while archiving
modbox zip -x '*.log' -r archive.zip src/

# Build an archive from a pipeline of names
printf 'a.txt\nb.txt\n' | modbox zip -@ archive.zip

# Compress with maximum speed
modbox zip -1 fast.zip largefile.bin

# Compress with maximum size reduction
modbox zip -9 small.zip largefile.bin
```

# EXIT STATUS

`0`
:   Archive created or updated successfully.

non-zero
:   At least one input could not be processed, the archive could not be
    created, or invalid options were supplied.

# SEE ALSO

**modbox-unzip**(1), **modbox-gzip**(1), **modbox-xz**(1), **modbox**(1)
