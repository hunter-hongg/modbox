% MODBOX-CHROOT(1) modbox | User Commands
% modbox project
% 2026-08-14

# NAME

modbox-chroot - run a command with a new root directory

# SYNOPSIS

**modbox chroot** [*OPTION*]... *NEWROOT* [*COMMAND* [*ARG*]...]

# DESCRIPTION

Run COMMAND with the root directory set to NEWROOT. NEWROOT is required. If no
COMMAND is given, modbox chroot runs **/bin/sh** in the new root.

modbox chroot must be run as root (it calls chroot(2), which requires
privileges). After changing root it optionally drops privileges via
**--userspec** and sets supplementary groups via **--groups** before exec'ing
the command.

# OPTIONS

**--userspec=***USER***[:***GROUP***]**
:   Specify the user and optional group (name or numeric ID) to switch to
    after changing root. If GROUP is omitted, the user's primary group is
    used.

**--groups=***G_LIST*
:   Specify supplementary groups as a comma-separated list of group names or
    numeric gids. Applied before the root change.

**--skip-chdir**
:   Do not change the working directory to '/' after changing root.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Start an interactive shell in a new root
modbox chroot /srv/jail

# Run a specific command inside the new root
modbox chroot /srv/jail /bin/ls -la /

# Drop privileges to user 'nobody' inside the jail
modbox chroot --userspec=nobody /srv/jail /usr/bin/myapp

# Set supplementary groups and skip the chdir to '/'
modbox chroot --groups=users,audio --skip-chdir /srv/jail /bin/sh
```

# EXIT STATUS

On successful execution, modbox chroot does not return its own status: the
process is replaced by COMMAND, so the exit status seen by the caller is that
of COMMAND (for example `0` on success, `1` on a command error, etc.).

`127`
:   The command could not be executed (exec failure). modbox prints an error
    to stderr and exits 127.

Setup and usage errors — such as not running as root, an invalid
**--userspec** or **--groups** value, or failure to change the root directory
— are reported to stderr. In the current modbox implementation these cases
return `0` rather than a distinct failure code, so they cannot be distinguished
from success by exit status alone; check stderr for diagnostics.

# NOTES

- NEWROOT must exist and be a directory; it is mandatory.
- COMMAND defaults to **/bin/sh** when omitted.
- Because chroot(2) requires root, run modbox chroot as root (e.g. via sudo).

# SEE ALSO

**modbox-chmod**(1), **modbox-chown**(1), **modbox-chgrp**(1),
**modbox-chcon**(1), **modbox**(1)
