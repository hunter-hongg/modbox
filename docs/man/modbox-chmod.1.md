% MODBOX-CHMOD(1) modbox | User Commands
% modbox project
% 2026-08-08

# NAME

modbox-chmod - change file mode bits

# SYNOPSIS

**modbox chmod** [*OPTION*]... *MODE*[,*MODE*]... *FILE*...
**modbox chmod** [*OPTION*]... *OCTAL-MODE FILE*...
**modbox chmod** [*OPTION*]... **--reference=***RFILE* *FILE*...

# DESCRIPTION

Change the mode of each FILE to MODE. With **--reference**, change the mode
of each FILE to that of RFILE.

MODE can be specified symbolically (a combination of who, op, and permission
letters) or as an octal number.

# OPTIONS

**-c**, **--changes**
:   Like verbose, but report only when a change is made.

**-f**, **--silent**, **--quiet**
:   Suppress most error messages.

**-v**, **--verbose**
:   Output a diagnostic for every file processed.

**--no-preserve-root**
:   Do not treat '/' specially (the default).

**--preserve-root**
:   Fail to operate recursively on '/'.

**--reference=***RFILE*
:   Use RFILE's mode instead of MODE values.

**-R**, **--recursive**
:   Change files and directories recursively.

**-h**, **--help**
:   Display help and exit.

# SYMBOLIC MODE GRAMMAR

Each MODE is of the form:

    [ugoa]*([-+=]([rwxXst]*|[ugo]))+

- **who**: `u` (user/owner), `g` (group), `o` (other), `a` (all, default)
- **op**: `+` (add), `-` (remove), `=` (set exactly)
- **perm**: `r` (read), `w` (write), `x` (execute), `X` (execute only if
  directory or already executable), `s` (setuid/setgid), `t` (sticky),
  or a copy of another who's perms via `u`, `g`, `o`.

Multiple symbolic modes can be combined, comma-separated.

# OCTAL MODE

An octal MODE is one to four digits (0-7). Leading digits set setuid (4),
setgid (2), and sticky (1) bits; the last three digits set owner, group,
and other permissions respectively (4=read, 2=write, 1=execute).

# EXAMPLES

```bash
# Grant everyone read/write, remove execute for all
modbox chmod a=rw,ug+x file.txt

# Recursively make a directory tree readable/executable by all
modbox chmod -R a+rX /srv/share

# Set exact permissions via octal mode (rwxr-xr-x)
modbox chmod 755 script.sh

# Copy mode from another file
modbox chmod --reference=template.conf config.conf

# Report only files whose mode actually changed
modbox chmod -c 644 *.log
```

# EXIT STATUS

`0`
:   Success.

`1`
:   An error occurred (e.g. invalid mode, permission denied, missing
    operand, or recursive operation on '/' with **--preserve-root**).

# NOTES

- The **X** permission applies execute only to directories and to files
  that are already executable for some user, making it safe for
  recursive trees.
- **--preserve-root** prevents accidental recursive changes to the root
  filesystem.

# SEE ALSO

**modbox-chown**(1), **modbox-mkdir**(1), **modbox**(1)
