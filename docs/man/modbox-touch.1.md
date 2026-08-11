% MODBOX-TOUCH(1) modbox | User Commands
% modbox project
% 2026-08-11

# NAME

modbox-touch - update file access and modification times

# SYNOPSIS

**modbox touch** [*OPTION*]... *FILE*...

# DESCRIPTION

Update the access and modification times of each FILE to the current time.
If a FILE does not exist, it is created as an empty file (unless **-c** is
given).

# OPTIONS

**-a**
:   Change only the access time.

**-c**, **--no-create**
:   Do not create any files that do not already exist.

**-d**, **--date=STRING**
:   Parse STRING and use the resulting time instead of the current time.
    Supported formats: `YYYY-MM-DD HH:MM:SS`, `YYYY-MM-DD`,
    `YYYY/MM/DD HH:MM:SS`, `YYYY/MM/DD`, `MM/DD/YYYY HH:MM:SS`, `MM/DD/YYYY`.

**-m**
:   Change only the modification time.

**-r**, **--reference=FILE**
:   Use the access and modification times of FILE instead of the current time.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Update timestamps of existing files
modbox touch file1.txt file2.txt

# Create a new empty file
modbox touch newfile.txt

# Do not create missing files
modbox touch -c missing.txt

# Use reference file's timestamps
modbox touch -r existing.txt newfile.txt

# Set a specific date
modbox touch -d "2026-01-01" file.txt

# Change only access time
modbox touch -a file.txt
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- By default, touch creates files that do not exist. Use **-c** to prevent this.
- When both **-a** and **-m** are given, the last one wins.
- The **--date** option accepts a limited set of date formats; arbitrary
  natural language parsing is not supported.

# SEE ALSO

**modbox-date**(1), **modbox-stat**(1), **modbox**(1)
