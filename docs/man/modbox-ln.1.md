% MODBOX-LN(1) modbox | User Commands
% modbox project
% 2026-08-08

# NAME

modbox-ln - create hard and symbolic links

# SYNOPSIS

**modbox ln** [*OPTION*]... *SOURCE* *DEST*
**modbox ln** [*OPTION*]... *SOURCE* *DIRECTORY*


# DESCRIPTION

Create a link to SOURCE with the name DEST, or create links in DIRECTORY
to each SOURCE. By default, hard links are created. Use **-s** to create
symbolic links.

For hard links, SOURCE must exist and be a regular file. For symbolic
links, SOURCE need not exist.

If DEST is an existing directory, a link named after SOURCE's basename is
created inside that directory. Exactly one SOURCE is accepted.


# OPTIONS

**-v**, **--verbose**
:   Explain what is being done (print "'DEST' -> 'SOURCE'").

**-f**, **--force**
:   Remove an existing destination file before linking.

**-s**, **--symbolic**
:   Make symbolic links instead of hard links.

**-i**, **--interactive**
:   Prompt before overwriting an existing destination. The prompt is
    shown on */dev/tty* when available, falling back to stdin.

**-n**, **--no-dereference**
:   Do not dereference DEST if it is a symbolic link to a directory.
    Without this, a link would be created inside the direc**-L**, **--logical**
:   When creating hard links, dereference SOURCE symbolic links first
    (link to the target, not the link itself).
 flag; pass **-h** to see usage.)

# EXAMPLES

```bash
# Create a hard link
modbox ln a.txt b.txt

# Create a symbolic link
modbox ln -s /var/log/app.log app.log

# Force overwrite an existing link
modbox ln -sf target linkname

# Create links for many sources inside a directory
modbox ln -s /usr/bin/* /local/bin/

# Verbose symbolic link creation
modbox ln -sv config.conf config.link
```

# EXIT STATUS

`0`
:   Success (or a skipped interactive prompt answered "no").

`1`
:   Returned by the argument parser when an invalid or missing option is
    given (e.g. unknown flag).

`0`
:   Otherwise, including when SOURCE is missing for a hard link, SOURCE is
    not a regular file, or the user declines an interactive prompt. The
    error is still printed to stderr; only the exit status is 0.


# NOTES

- Hard links cannot span filesystems and cannot point to directories.
- If **-i** is given and the user declines, the link is skipped without
  error.
- **-n** is important when DEST is a symlink: it keeps the new link at
  DEST instead of inside the directory DEST references.

# SEE ALSO

**modbox-cp**(1), **modbox-mv**(1), **modbox-rm**(1), **modbox**(1)
