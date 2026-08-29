% MODBOX-RESTORECON(1) modbox | User Commands
% modbox project
% 2026-08-29

# NAME

modbox-restorecon - restore SELinux security context of files

# SYNOPSIS

**modbox restorecon** [*OPTION*]... *FILE*...

# DESCRIPTION

Restore the default SELinux security context of each FILE according to the
policy's file-context configuration. modbox restorecon requires SELinux support
on the host and on the target filesystem.

# OPTIONS

**-R**, **--recursive**
:   Operate on files and directories recursively.

**-v**, **--verbose**
:   Output a diagnostic for every file processed (`relabeled PATH from OLD to NEW`).

**-n**, **--nochange**
:   Do not change any file labels; report what would be relabeled.

**-F**, **--force**
:   Force reset of customizable and default contexts (equivalent to forcing a
    full context, not just the type component).

**-i**, **--ignore-errors**
:   Continue relabeling the rest of the tree after a per-file error instead of
    aborting the run.

**--help**
:   Display help and exit (long option only).

**--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Relabel a single file to its policy default
modbox restorecon /srv/www/index.html

# Recursively and verbosely relabel a directory tree
modbox restorecon -Rv /etc/custom.d

# Dry-run: show what would change without touching disk
modbox restorecon -Rn /srv/app
```

# EXIT STATUS

`0`
:   All requested files were relabeled successfully.

`1`
:   An error occurred (e.g. invalid invocation, missing operand, or a SELinux
    operation failure). With `-i`, a per-file error is reported but the run
    continues; the exit status is still non-zero if any file failed.

# NOTES

- modbox restorecon requires SELinux support. On a host or filesystem without
  SELinux, each file is reported as a relabel failure on stderr.
- Each FILE is relabeled independently; supplying multiple operands relabels
  them all in one invocation.

# SEE ALSO

**modbox-chcon**(1), **modbox-getenforce**(1), **modbox**(1)
