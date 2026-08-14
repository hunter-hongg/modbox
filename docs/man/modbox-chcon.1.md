% MODBOX-CHCON(1) modbox | User Commands
% modbox project
% 2026-08-14

# NAME

modbox-chcon - change SELinux security context of files

# SYNOPSIS

**modbox chcon** [*OPTION*]... *CONTEXT FILE*...

**modbox chcon** [*OPTION*]... **-u** *USER* [*-r* *ROLE*] [*-t* *TYPE*] [*-l* *RANGE*] *FILE*...

**modbox chcon** [*OPTION*]... **--reference=***RFILE FILE*...

# DESCRIPTION

Change the SELinux security context of each FILE to CONTEXT. modbox chcon
requires SELinux support on the host and on the target filesystem.

Three invocation forms are supported:

1. **Full CONTEXT**: the first non-option argument is a complete context
   string (e.g. `system_u:object_r:etc_t:s0`), followed by the files to change.
2. **Component form**: one or more of `-u`, `-r`, `-t`, `-l` specify the user,
   role, type, and range components of the context.
3. **Reference form**: `--reference=***RFILE* copies the security context from
   RFILE.

# OPTIONS

**-R**, **--recursive**
:   Operate on files and directories recursively.

**-v**, **--verbose**
:   Output a diagnostic for every file processed.

**-h**, **--no-dereference**
:   Affect symbolic links instead of their targets.

**--preserve-root**
:   Fail to operate recursively on '/'.

**--no-preserve-root**
:   Do not treat '/' specially (the default).

**-u**, **--user=***USER*
:   Set the user component of the target security context.

**-r**, **--role=***ROLE*
:   Set the role component of the target security context.

**-t**, **--type=***TYPE*
:   Set the type (domain) component of the target security context.

**-l**, **--range=***RANGE*
:   Set the range (MLS/MCS level) component of the target security context.

**--reference=***RFILE*
:   Use RFILE's security context instead of specifying one.

**--help**
:   Display help and exit (long option only).

# EXAMPLES

```bash
# Set a full context on a file
modbox chcon system_u:object_r:httpd_sys_content_t:s0 /srv/www/index.html

# Set only the type component
modbox chcon -t httpd_sys_content_t /srv/www/index.html

# Copy the context from another file
modbox chcon --reference=/etc/passwd /etc/passwd.new

# Recursively relabel a directory tree, following no symlinks
modbox chcon -R -h -t etc_t /etc/custom.d
```

# EXIT STATUS

`0`
:   The command completed (modbox reports per-file failures, such as a file
    without a security context or lacking SELinux support, to stderr).

`1`
:   An error occurred (e.g. invalid invocation, missing operand, or a SELinux
    operation failure).

# NOTES

- modbox chcon requires SELinux support. On a host or filesystem without
  SELinux, or for files that do not have a security context, the operation is
  skipped and an error is reported to stderr for that file.
- When no `-u/-r/-t/-l` or `--reference` is given, the first positional
  argument is treated as a full CONTEXT string and the remaining arguments are
  the files to relabel. At least one file must be supplied.

# SEE ALSO

**modbox-chgrp**(1), **modbox-chown**(1), **modbox-chmod**(1),
**modbox-chattr**(1), **modbox**(1)
