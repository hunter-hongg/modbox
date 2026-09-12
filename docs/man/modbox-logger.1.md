---
title: modbox-logger
section: 1
date: 2026-09-12
author: modbox project
---

# NAME

modbox-logger - enter messages into the system log

# SYNOPSIS

**modbox logger** [**OPTION**]... [**MESSAGE**]

# DESCRIPTION

**logger** writes a message to the system log. With no **MESSAGE**
argument, the message is read from standard input. The message is
submitted through the POSIX **syslog**(3) interface, so it is delivered
wherever the system's syslog daemon is configured to route it.

Whether the message is actually recorded depends on the host's syslog
configuration; **logger** does not fail when no syslog daemon is
running.

# OPTIONS

**-f**, **--file** *file*
:   Log the contents of *file*. A trailing newline is stripped. A *file*
    of `-` reads from standard input. This option is mutually exclusive
    with a **MESSAGE** argument.

**-i**, **--id**
:   Log the process ID of the **logger** process with each message.

**-p**, **--priority** *pri*
:   Set the message priority. *pri* is either a bare severity or a
    `facility.severity` pair. The default is `user.notice`.

**-s**, **--stderr**
:   Write the message to standard error as well as to the system log.

**-t**, **--tag** *tag*
:   Mark every line of the message with the specified *tag*.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# PRIORITIES

Valid facilities are `auth`, `authpriv`, `cron`, `daemon`, `ftp`,
`kern`, `lpr`, `mail`, `news`, `syslog`, `user`, `uucp`, and
`local0` through `local7`.

Valid severities, from least to most severe, are `debug`, `info`,
`notice`, `warning`, `err`, `crit`, `alert`, and `emerg`.

# EXAMPLES

```bash
# Log a plain message
modbox logger "service restarted"

# Log with a facility and severity, and echo to stderr
modbox logger -s -p local0.warning -t myapp "disk usage high"

# Log the contents of a file
modbox logger -f /var/log/import.log

# Log standard input
tail -n 1 app.log | modbox logger -t app
```

# EXIT STATUS

`0` on success. `1` on an unreadable `--file`, an unrecognized
priority, a missing option argument, or an unknown option.

# NOTES

- A syslog daemon must be running for messages to be stored; `logger`
  itself always exits successfully when the message is submitted.
- The **--stderr** option is the most reliable way to observe the exact
  message that **logger** submits, since it does not depend on the
  syslog configuration.

# SEE ALSO

**modbox-wall**(1), **modbox-who**(1), **modbox-audit2allow**(1), **modbox-ausearch**(1)
