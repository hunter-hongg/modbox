% MODBOX-FLOCK(1) modbox | User Commands
% modbox project
% 2026-09-17

# NAME

modbox-flock - Manage file locks from shell scripts

# SYNOPSIS

**modbox flock** [*OPTIONS*] *file*|*directory* [*command* [*arg*...]]
**modbox flock** [*OPTIONS*] *file*|*directory* **-c** *command*
**modbox flock** [*OPTIONS*] *file-descriptor*

# DESCRIPTION

**flock** applies or removes an advisory lock on an open file, and optionally
runs a command while the lock is held. The lock is automatically released when
the command exits, or when the file descriptor is closed (fd form).

The first form (two or more non-option arguments) opens *file* (creating it if
necessary) and runs *command* with its arguments.

The second form opens *file* and runs *command* through the shell (`$SHELL -c`
or `/bin/sh -c` if SHELL is unset or empty).

The third form (exactly one non-option argument) treats the argument as a
numeric file descriptor and applies the lock directly to that descriptor.

# OPTIONS

`-s`, `--shared`
:   Obtain a shared lock (LOCK_SH). Multiple processes may hold a shared lock
    simultaneously.

`-x`, `--exclusive`
:   Obtain an exclusive lock (LOCK_EX). This is the default.

`-e`
:   Alias for `--exclusive` (upstream legacy).

`-u`, `--unlock`
:   Drop an existing lock. A lock is also dropped when the file descriptor is
    closed.

`-n`, `--nonblocking`, `--nb`
:   Fail immediately if the lock cannot be obtained, rather than waiting.

`-w`, `--wait`, `--timeout` *seconds*
:   Wait up to *seconds* for the lock. Fractional values are accepted. A
    timeout of zero is equivalent to `-n` (no timer is armed). On timeout the
    command exits with the conflict exit code (see `-E`).

`-E`, `--conflict-exit-code` *code*
:   Exit status to use when a lock conflict or timeout occurs (default 1).
    Must be in the range 0..255.

`-o`, `--close`
:   Close the lock file descriptor before executing *command*. The lock is
    retained by the waiting parent until the command exits. Incompatible with
    `--no-fork`.

`-c`, `--command` *command*
:   Pass a single command string to the shell (`$SHELL -c` or `/bin/sh -c`).

`-F`, `--no-fork`
:   Execute *command* in place (without forking), replacing the flock process.
    The command inherits the lock descriptor unless `--close` is given.
    Incompatible with `--close`.

`--verbose`
:   Print diagnostic messages to stdout: the time spent waiting for the lock
    (`getting lock took X.XXXXXX seconds`, measured with a monotonic clock)
    and the command being executed (`executing ...`).

`-h`, `--help`
:   Display the usage message and exit.

`-V`, `--version`
:   Print version information and exit.

# POSITIONAL ARGUMENTS

0 arguments
:   Usage error ("not enough arguments").

1 argument
:   **fd form**. The argument must be a valid numeric file descriptor. The
    lock is applied to that descriptor; no command is run.

2 or more arguments
:   **file form**. The first argument is the lock file path; the remaining
    arguments are the command and its arguments. If the second argument is
    `-c` or `--command`, exactly one more argument (the shell command string)
    is accepted.

# EXAMPLES

Run a command with an exclusive lock on a file:

```sh
$ modbox flock /var/lock/myapp.lock ./do-work
```

Run a shell command with a shared lock, failing immediately if busy:

```sh
$ modbox flock -s -n /var/lock/myapp.lock -c 'echo "locked"'
```

Wait up to 10 seconds for a lock, with a custom conflict exit code:

```sh
$ modbox flock -w 10 -E 7 /var/lock/myapp.lock ./do-work
```

Lock a file descriptor inherited from the parent (no fork):

```sh
$ exec 9>/var/lock/myapp.lock
$ modbox flock -F 9
```

Apply a lock, then drop it explicitly (fd form):

```sh
$ exec 9>/var/lock/myapp.lock
$ modbox flock 9
$ modbox flock -u 9
```

Verbose diagnostics:

```sh
$ modbox flock --verbose /tmp/lock true
flock: getting lock took 0.000004 seconds
flock: executing true
```

# EXIT STATUS

`0`
:   The lock was obtained (and released if a command was run), and the command
    exited successfully.

`1`..`255` (default 1)
:   Lock conflict or timeout (set by `--conflict-exit-code`).

`1`
:   Runtime error (cannot open lock file, invalid fd, fork/exec failure other
    than exec failure).

`127`
:   Failed to execute the command (repo convention; upstream uses 69).

`2`
:   Usage error (bad option, missing operand, invalid timeout, invalid
    conflict-exit-code, `--close` with `--no-fork`, etc.).

# NOTES

Upstream **flock** is part of util-linux. Intentional deviations:

- Usage errors exit with status 2 instead of `EX_USAGE` (64).
- A failed `exec` exits with status 127 instead of 69 (repo launcher convention).
- Error wording follows the repository standard: `unrecognized option ...` and
  `Try 'flock --help' for more information.`
- The OFD-lock options (`--fcntl`, `--start`, `--length`) are not implemented
  and are rejected as unrecognized options.

# SEE ALSO

**flock**(2), **fcntl**(2), **sh**(1)