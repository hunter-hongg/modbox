% MODBOX-CHGRP(1) modbox | User Commands
% modbox project
% 2026-08-14

# NAME

modbox-chgrp - change group ownership of files

# SYNOPSIS

**modbox chgrp** [*OPTION*]... *GROUP FILE*...

**modbox chgrp** [*OPTION*]... **--reference=***RFILE FILE*...

# DESCRIPTION

Change the group ownership of each FILE to GROUP. GROUP may be a group name
or a numeric group ID (gid). With **--reference**, change the group of each
FILE to that of RFILE.

# OPTIONS

**-c**, **--changes**
:   Like verbose, but report only when a change is made.

**-f**, **--silent**, **--quiet**
:   Suppress most error messages.

**-v**, **--verbose**
:   Output a diagnostic for every file processed.

**--dereference**
:   Affect the referent of each symbolic link (the default).

**-h**, **--no-dereference**
:   Affect symbolic links instead of any referent.

**--reference=***RFILE*
:   Use RFILE's group instead of specifying a GROUP value.

**-R**, **--recursive**
:   Operate on files and directories recursively.

**--preserve-root**
:   Fail to operate recursively on '/'.

**--no-preserve-root**
:   Do not treat '/' specially (the default).

**-H**
:   If **-R** is given, follow symbolic links on the command line.

**-L**
:   If **-R** is given, follow all symbolic links.

**-P**
:   If **-R** is given, do not follow any symbolic links (the default).

**--help**
:   Display help and exit (long option only).

# EXAMPLES

```bash
# Change group by name
modbox chgrp staff report.txt

# Change group by numeric gid
modbox chgrp 1000 report.txt

# Recursively change group, following no symlinks
modbox chgrp -R -P developers /srv/project

# Copy group from another file
modbox chgrp --reference=/etc/passwd /etc/passwd.new

# Report only files whose group actually changed
modbox chgrp -c www-data /srv/www/*
```

# EXIT STATUS

`0`
:   Success.

`1`
:   An error occurred (e.g. invalid group, permission denied, missing operand,
    or recursive operation on '/' with **--preserve-root**).

# NOTES

- When **-R** is used, the symlink-following behavior is controlled by **-H**,
  **-L**, and **-P**. These options are only meaningful together with **-R**;
  the default is **-P** (follow no symbolic links).
- GROUP is resolved as a numeric gid when it consists solely of digits,
  otherwise as a group name via the system group database.

# SEE ALSO

**modbox-chown**(1), **modbox-chmod**(1), **modbox-chcon**(1),
**modbox-chattr**(1), **modbox**(1)
