---
title: modbox-kill
section: 1
date: 2026-08-19
author: modbox project
---

# NAME

modbox-kill - terminate a process

# SYNOPSIS

**modbox kill** [**-s SIGNAL**]... PID...

# DESCRIPTION

Send the specified signal to each listed PID.
If no signal is specified, sends SIGTERM.

# OPTIONS

**-s**, **--signal=SIGNAL**
:   Send the named signal instead of SIGTERM.
    SIGNAL may be a name like `HUP` or a number like `1`.

**-l**, **--list**
:   List all signal names.

**-L**, **--table**
:   List all signal names in table format.

**-p**, **--pid**
:   Only print PID of each process, do not send signal.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# SIGNALS

Common signals:

| Signal | Number | Description |
|--------|--------|-------------|
| SIGHUP | 1 | Hangup detected on controlling terminal |
| SIGINT | 2 | Interrupt from keyboard |
| SIGQUIT | 3 | Quit from keyboard |
| SIGKILL | 9 | Kill signal (cannot be caught) |
| SIGTERM | 15 | Termination signal (default) |
| SIGCONT | 18 | Continue execution |
| SIGSTOP | 19 | Stop execution |

# EXAMPLES

```bash
# Send SIGTERM to process 1234
modbox kill 1234

# Send SIGKILL to force terminate
modbox kill -9 1234

# Send HUP signal to reload configuration
modbox kill -HUP 1234

# Print PID without sending signal
modbox kill -p 1234

# List all available signals
modbox kill -l
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- SIGKILL (9) cannot be caught or ignored.
- SIGTERM (15) is the default signal and allows graceful shutdown.
- Use **kill -l** to see all signal names.

# SEE ALSO

**modbox-killall**(1), **modbox-pkill**(1), **modbox-timeout**(1)
