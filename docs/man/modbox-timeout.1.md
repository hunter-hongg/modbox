---
title: modbox-timeout
section: 1
date: 2026-08-19
author: modbox project
---

# NAME

modbox-timeout - run a command with a time limit

# SYNOPSIS

**modbox timeout** [*OPTION*] **DURATION** **COMMAND** [*ARG*]...

# DESCRIPTION

Start **COMMAND**, and kill it if it is still running after **DURATION**.

# OPTIONS

**--preserve-status**
:   Exit with the same status as **COMMAND**, even when the command times out.

**--foreground**
:   When not running timeout directly from a shell prompt, allow **COMMAND** to read from the TTY and get TTY signals. In this mode, children of **COMMAND** will not be timed out.

**--kill-after=DURATION**
:   Also send a KILL signal if **COMMAND** is still running this long after the initial signal was sent.

**--signal=SIGNAL**
:   Specify the signal to be sent on timeout. SIGNAL may be a name like `HUP` or a number. See `kill -l` for a list of signals. Default is SIGTERM.

**-v**, **--verbose**
:   Diagnose to stderr any signal sent upon timeout.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# DURATION FORMAT

DURATION is a floating point number with an optional suffix:
- `s` for seconds (the default)
- `m` for minutes
- `h` for hours
- `d` for days

# EXIT STATUS

| Status | Description |
|--------|-------------|
| 0 | Command completed successfully |
| 124 | Command timed out (unless `--preserve-status` is set) |
| 125 | Timeout itself failed |
| 126 | Command invoked cannot execute |
| 127 | Command not found |

# EXAMPLES

```bash
# Run command with 5 second timeout
modbox timeout 5 sleep 10

# Run with verbose output
modbox timeout -v 10 mycommand

# Preserve exit status on timeout
modbox timeout --preserve-status 5 slow_command

# Kill after sending signal
modbox timeout --kill-after=5 10 slow_command
```

# NOTES

- If **COMMAND** times out, it receives SIGTERM by default.
- Use `--preserve-status` to return the command's exit status even on timeout.
- Use `--signal=KILL` to forcefully terminate on timeout.

# SEE ALSO

**modbox-kill**(1), **modbox-bash**(1), **modbox-sh**(1)
