# NAME
modbox-pidof - list the process IDs of running programs

# SYNOPSIS
**modbox pidof** \[OPTIONS\] PROGRAM...

# DESCRIPTION
Looks up the running processes whose program (executable) name matches one
of the given PROGRAMs and prints their process IDs on a single line, highest
PID (i.e. most recently started) first.

A PROGRAM argument containing a slash is matched against the exact executable
path of each process (read from /proc/[pid]/exe); a bare name is matched
against the basename of the executable, falling back to the process comm name
when the executable link is unreadable. Kernel threads (which have no
executable link and no command line) are matched by comm only, like the
system **pidof**.

The pidof process never lists itself.

# OPTIONS
**-s**, **--single-shot**
:   Return one PID only (the highest).

**-q**
:   Quiet mode: produce no output; only the exit code indicates whether any
    process matched.

**-x**
:   Also find shells running the named scripts. A process whose first command
    line argument is one of the common shells (sh, bash, dash, ash, ksh, zsh,
    csh, tcsh, fish) matches when the first non-option argument of that shell
    has the basename of PROGRAM.

**-o**, **--omit-pid=PID**
:   Omit the given PID from the output. The option may be repeated, the value
    may be a comma-separated list of PIDs, and the special value **%PPID**
    (or **PPID**) omits the parent of the pidof process.

**-S**, **--separator=SEP**
:   Use SEP as separator between PIDs instead of a single space.

**-h**, **--help**
:   Display a usage message and exit.

**-V**, **--version**
:   Output version information and exit.

# EXIT STATUS
:   0 if at least one PID is listed.
:   1 if the final PID list is empty — either nothing matched or every match
    was removed by **--omit-pid** — or on a usage error (no program name,
    invalid **--omit-pid** value, unknown option).

# EXAMPLES
List the PIDs of all running bash processes:

    pidof bash

List PIDs of two programs at once, newest first:

    pidof nginx sshd

Get only the most recently started instance:

    pidof -s httpd

Omit the parent shell (a common idiom in init scripts):

    pidof -o %PPID sendmail

Use a comma as separator instead of spaces:

    pidof -S, bash

# NOTES
Unlike procps-ng pidof, the options **-c** (check-root), **-w**
(with-workers) and **-t** (lightweight, list threads) are not implemented;
they are accepted by neither the short nor long form. Separator values are
taken verbatim, including multi-character strings.

Matching uses the executable path only (`/proc/PID/exe` basename, or the
exact path when PROGRAM contains a slash), falling back to comm when the
exe link is unreadable. Unlike procps-ng pidof, the **argv0** entry
(`/proc/PID/cmdline` field 0) is not consulted, so processes started via
`exec -a` with an overridden argv0, or matched via a symlinked interpreter
name such as `python3` (whose exe is `python3.14`), are not found under
those names.

With multiple PROGRAM arguments, output is one merged, deduplicated list
sorted highest-PID-first, rather than upstream's per-program blocks
concatenated in argument order with duplicates preserved when an argument
is repeated; and **-s** prints the single highest PID across all
arguments, rather than one PID per argument.

modbox pidof never lists itself, even though procps-ng pidof has no
self-exclusion (upstream lists its own process when asked for its own
name). Unlike procps-ng pidof, an **--omit-pid** value that is not a
positive decimal PID at or below the kernel's hard pid_max limit, or
**%PPID**, is a hard error (exit 1) rather than upstream's warn-and-
continue: upstream accepts any strtoul-parsable chunk as a legal omit
entry — including 0 (a no-op, since no live process has PID 0) and
values above pid_max — and only warns `illegal omit pid value (X)!`
on garbage while continuing.

The **--omit-pid** range check rejects PIDs above the kernel's hard
pid_max limit (4194304) but does not verify that a PID exists.

# SEE ALSO
modbox-pgrep(1), modbox-ps(1), modbox-kill(1)
