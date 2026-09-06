# NAME
modbox-pgrep - list process IDs of processes matching criteria

# SYNOPSIS
**modbox pgrep** \[OPTIONS\] PATTERN

# DESCRIPTION
Searches for processes whose name or full command line matches PATTERN and
prints their process IDs, one per line.

By default the pattern is matched as a substring against the process name
(comm). With **--full** the match is against the complete command line. With
**--exact** the process name must match the pattern exactly.

**--user** restricts matches to processes owned by the given user name or
numeric user ID.

# OPTIONS
**-l**, **--list-name**
:   List process ID and process name (comm).

**-a**, **--list-full**
:   List process ID and full command line.

**-f**, **--full**
:   Match pattern against the full command line instead of the process name.
    Kernel threads with an empty command line fall back to matching by
    process name.

**-i**, **--ignore-case**
:   Perform case-insensitive matching.

**-v**, **--invert-match**
:   Select processes that do not match.

**-x**, **--exact**
:   Match the process name exactly (no substring matching).

**-u**, **--user** USER
:   Only match processes owned by USER (name or numeric ID).

**-c**, **--count**
:   Print only the number of matching processes.

**-h**, **--help**
:   Display a usage message and exit.

**--version**
:   Output version information and exit.

# EXIT STATUS
:   0 if at least one process matched.
:   1 if no process matched.
:   2 for usage errors (missing pattern, duplicate pattern, unknown option,
    invalid user).

# EXAMPLES
Find the PIDs of all sleep processes:

    pgrep sleep

Show PIDs with full command lines:

    pgrep -a sshd

Match against the full command line:

    pgrep -f 'nginx -c /etc/nginx'

Count matching processes:

    pgrep -c bash

# NOTES
Unlike procps pgrep, the pattern is treated as a plain substring (or exact
string with **--exact**) rather than an extended regular expression.

Processes whose command line is empty (kernel threads) are matched by
process name, and **--list-full** prints the process name for them.

# SEE ALSO
modbox-ps(1), modbox-kill(1)
