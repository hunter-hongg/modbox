% MODBOX-CHOWN(1) modbox | User Commands
% modbox project
% 2026-08-08

# NAME

modbox-chown - change file owner and group

# SYNOPSIS

**modbox chown** [*OPTION*]... [*OWNER*][*:*[*GROUP*]] *FILE*...
**modbox chown** [*OPTION*]... **--reference=***RFILE* *FILE*...

# DESCRIPTION

Change the owner and/or group of each FILE to OWNER and/or GROUP. With
**--reference**, change the owner and group of each FILE to those of RFILE.

If OWNER is omitted, the group is changed. If GROUP is omitted, the group of
each file is not changed. If a colon but no group follows, the group is
changed to OWNER's login group.

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

**--from=***CURRENT_OWNER*:*CURRENT_GROUP*
:   Change the owner/group of each file only if its current owner/group
    matches those specified here.

**--no-preserve-root**
:   Do not treat '/' specially (the default).

**--preserve-root**
:   Fail to operate recursively on '/'.

**--reference=***RFILE*
:   Use RFILE's owner and group instead of specifying OWNER:GROUP values.

**-R**, **--recursive**
:   Operate on files and directories recursively.

**-H**
:   If **-R** is given, follow symbolic links on the command line.

**-L**
:   If **-R** is given, follow all symbolic links.

**-P**
:   If **-R** is given, do not follow any symbolic links (the default).

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Change owner only
modbox chown alice file.txt

# Change owner and group
modbox chown alice:staff file.txt

# Change group only (colon, no owner)
modbox chown :staff file.txt

# Copy ownership from another file
modbox chown --reference=template.conf config.conf

# Recursively change ownership, following no symlinks
modbox chown -R -P alice:staff /srv/app

# Only change files currently owned by bob:users
modbox chown --from=bob:users alice:staff /data/*.log
```

# EXIT STATUS

`0`
:   Success.

`1`
:   An error occurred (e.g. invalid user/group, permission denied,
    missing operand, or recursive operation on '/' with
    **--preserve-root**).

# NOTES

- When **-R** is used, the symlink-following behavior is controlled by
  **-H**, **-L**, and **-P**. The default is **-P** (no following).
- **--from** lets you narrow changes to files matching a current
  owner/group pair, reducing the risk of sweeping unintended files.

# SEE ALSO

**modbox-chmod**(1), **modbox-mkdir**(1), **modbox**(1)
