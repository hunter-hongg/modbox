% MODBOX-RENICE(1) modbox | User Commands
% modbox project
% 2026-09-12

# NAME

modbox-renice - Alter priority of running processes

# SYNOPSIS

**modbox renice** [*options*] *priority* [**-g**|**-p**|**-u**] *identifier*...

# DESCRIPTION

**modbox renice** alters the scheduling priority of one or more running
processes. The first argument is the *priority* value to be used. The other
arguments are interpreted as process IDs (by default), process group IDs,
user IDs, or user names. Renice'ing a process group causes all processes in
the process group to have their scheduling priority altered. Renice'ing a
user causes all processes owned by the user to have their scheduling
priority altered.

By default, *priority* is understood as an absolute value. With **--relative**,
or with **-n** when the environment variable **POSIXLY_CORRECT** is set, it is
understood as a relative value: the affected processes' niceness is
incremented/decremented by the given delta.

Niceness values range from **-20** (most favorable to the process) to **19**
(least favorable). The kernel clamps values outside this range, and the
change reported is the clamped value.

For each target that is changed, a line of the form

    <id> (<type>) old priority <old>, new priority <new>

is printed, where `<type>` is `process ID`, `process group ID`, or `user ID`.

# OPTIONS

`-n`, `--priority` *priority*
:   Specify the **absolute** scheduling priority, unless **POSIXLY_CORRECT**
    is set in the environment, in which case it is **relative**.

`--relative` *delta*
:   Specify a **relative** priority. The affected processes' scheduling
    priority is incremented/decremented by *delta*.

`-p`, `--pid`
:   Interpret the following identifiers as process IDs (the default).

`-g`, `--pgrp`
:   Interpret the following identifiers as process group IDs.

`-u`, `--user`
:   Interpret the following identifiers as user names or numeric user IDs.

`-h`, `--help`
:   Display help text and exit.

`-V`, `--version`
:   Print version information and exit.

# EXIT STATUS

`0` when every identifier was processed successfully, `1` when any
identifier failed (no such process, permission denied, unknown user,
malformed identifier) or when the arguments are insufficient or invalid.
Successful changes are still reported on standard output even when other
identifiers fail.

# NOTES

Users other than the superuser may alter the priority only of processes
they own. Furthermore, an unprivileged user can only increase the niceness
value (lower the urgency), and such changes are irreversible unless the
user has a suitable **nice** resource limit. Changing a *user's* priority
via **-u** requires privilege even for one's own processes.

# EXAMPLES

Set process 1234 to an absolute niceness of 10:

    modbox renice 10 -p 1234

Increase process 1234's niceness by 5:

    modbox renice --relative 5 -p 1234

Lower the priority of every process in process group 4242:

    modbox renice 19 -g 4242

# SEE ALSO

**modbox-nice**(1), **modbox**(1)
