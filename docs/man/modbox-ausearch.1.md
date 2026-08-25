% MODBOX-AUSEARCH(1) modbox | User Commands
% modbox project
% 2026-08-25

# NAME

modbox-ausearch - search the audit log for events

# SYNOPSIS

**modbox ausearch** [*OPTIONS*]

# DESCRIPTION

Search audit log records for events matching the specified filters.
The default behavior reads from standard input, making it easy to pipe
output from tools such as **auditd** logs or other audit sources.

ausearch parses audit log lines containing `msg=audit(...)` stamps and
assembles them into events, then applies filters and outputs matching
events in a human-readable format.

# OPTIONS

## Input sources

`-i` *FILE*, `--input`=\ *FILE*
:   Read input from the specified FILE instead of stdin.

`--input-logs`
:   Force using auditd log locations (not yet supported in modbox v1).

## Event identification

`-a` *NUM*, `--event`=\ *NUM*
:   Search by event ID (requires libaudit). Not supported in modbox v1.

`-m` *TYPE*, `--message`=\ *TYPE*
:   Search by message type (e.g., `SYSCALL`, `AVC`, `USER_LOGIN`).
    `ALL` matches every type. If called with `-m` and no argument,
    lists valid message types.

`-n` *NAME*, `--node`=\ *NAME*
:   Search by node name.

## User/Process filters

`--uid`=\ *NUM*
:   Real UID. Accepts a number or a user name.

`--uid-effective`=\ *NUM*
:   Effective UID. Accepts a number or a user name.

`--uid-all`=\ *NUM*
:   Match *NUM* in any UID field (uid, euid, auid, suid).

`--loginuid`=\ *NUM*
:   Login UID (auid). Accepts a number or a user name.

`--gid`=\ *NUM*
:   Real GID. Accepts a number or a group name.

`--gid-effective`=\ *NUM*
:   Effective GID. Accepts a number or a group name.

`--gid-all`=\ *NUM*
:   Match *NUM* in any GID field (gid, egid, sgid).

`-p` *NUM*, `--pid`=\ *NUM*
:   Process ID.

`--ppid`=\ *NUM*
:   Parent PID.

`-c` *NAME*, `--comm`=\ *NAME*
:   Command name (e.g., `ls`, `systemd`).

`-x` *PATH*, `--executable`=\ *PATH*
:   Executable path.

## Syscall filters

`--syscall`=\ *NAME*
:   Syscall name (e.g., `open`, `execve`) or number (e.g., `59`).
    The name is resolved based on the architecture of the event using
    the host kernel's syscall table.

`-e` *CODE*, `--exit`=\ *CODE*
:   Exit code.

`--arch`=\ *ARCH*
:   Architecture identifier (e.g., `c000003e` for x86_64, `c000003b`
    for i386). Accepts hex with or without `0x` prefix.

## Path/Key filters

`-f` *PATH*, `--file`=\ *PATH*
:   Filename or path.

`-k` *STRING*, `--key`=\ *STRING*
:   Audit rule key.

## SELinux filters

`--subject`=\ *CTX*
:   Subject context (scontext).

`-o` *CTX*, `--object`=\ *CTX*
:   Object context (tcontext).

`--context`=\ *CTX*
:   Match either subject or object context.

## Time filters

`--start`=\ *TIME*
:   Start time. Can be a Unix timestamp or a keyword:
    `now`, `recent` (10 minutes ago), `today`, `yesterday`,
    `this-hour`, `week-ago`, `this-week`, `this-month`,
    `this-year`, or `boot`. Also accepts `YYYY-MM-DD HH:MM:SS`
    or `YYYY-MM-DDTHH:MM:SS`.

`--end`=\ *TIME*
:   End time (same format as `--start`).

## Output

`--interpret`
:   Interpret mode: convert numeric UIDs, GIDs, syscall numbers, etc.,
    to human-readable names. (Alias for `--format=interpret`.)

`-r`, `--raw`
:   Raw output: print lines exactly as they appear in the input.

`--format`=\ *FORMAT*
:   Output format: `default` (event headers + lines),
    `interpret` (numeric to text), `raw` (no headers).

`-l`, `--line-buffered`
:   Line-buffered output (flush after each line).

`--just-one`
:   Stop after the first matching event.

`--word`
:   Enable whole-word matching for patterns.

## Other

`--host`=\ *NAME*
:   Hostname (matches `addr` or `host` fields).

`--terminal`=\ *TTY*
:   Terminal (matches `tty` field).

`--success`=\ *FLAG*
:   Filter by success flag (`yes` or `no`).

`-h`, `--help`
:   Display help and exit.

`-v`, `--version`
:   Output version information and exit.

# INPUT FORMAT

ausearch parses lines in the standard audit log format:

```
type=SYSCALL msg=audit(1680000000.123:456): arch=c000003e syscall=59 success=yes exit=0 a0=... comm="ls" exe="/bin/ls"
```

Each line must contain a `msg=audit(EPOCH.MSEC:SERIAL)` stamp to be
recognized as an audit record. Lines without this stamp are ignored.

Multiple records with the same `EPOCH.MSEC:SERIAL` stamp are assembled
into a single event. A new event starts whenever the stamp changes.

# OUTPUT FORMATS

## Default format

Each event is printed with a header line showing the timestamp:

```
---- time->Wed Mar 29 10:00:00 2023
type=SYSCALL msg=audit(1680000000.123:456): arch=c000003e syscall=59 success=yes exit=0 ...
type=AVC msg=audit(1680000000.123:456): avc:  denied  { read } for ...
```

## Raw format (`-r`)

Prints the raw input lines without any headers:

```
type=SYSCALL msg=audit(1680000000.123:456): arch=c000003e syscall=59 success=yes exit=0 ...
type=AVC msg=audit(1680000000.123:456): avc:  denied  { read } for ...
```

## Interpret format (`--format=interpret` or `--interpret`)

Same as default, but numeric UIDs, GIDs, and syscall numbers are
resolved to names where possible.

# EXAMPLES

```bash
# Read from a file
modbox ausearch -i /var/log/audit/audit.log

# Filter by message type
modbox ausearch -m SYSCALL -i audit.log

# Filter by UID and command
modbox ausearch --uid 1000 -c ls -i audit.log

# Filter by syscall name
modbox ausearch --syscall open -i audit.log

# Filter by PID
modbox ausearch -p 1234 -i audit.log

# Time-based filtering
modbox ausearch --start "2023-03-29 10:00:00" --end "2023-03-29 12:00:00" -i audit.log

# Raw output
modbox ausearch -r -m AVC -i audit.log

# Just one match
modbox ausearch --just-one -m SYSCALL -i audit.log

# Whole-word matching
modbox ausearch --word -c ls -i audit.log

# Pipe from ausearch (if using system audit)
ausearch -m avc | modbox ausearch -m AVC
```

# MESSAGE TYPES

Common message types include:

`SYSCALL`
:   System call records.

`AVC`
:   Access Vector Cache denials.

`USER_AVC`
:   User-space AVC entries.

`SELINUXERR`
:   SELinux error records.

`LOGIN`
:   Login/logout records.

`USER_AUTH`
:   User authentication.

`KERNEL`
:   Kernel messages.

For a complete list, run **modbox ausearch -m** with no argument.

# NOTES

- The parser treats `msg=audit(...)` stamps as the primary event
  identifier. Events are grouped by the stamp's epoch and serial.
- Lines without a valid audit stamp are discarded.
- Syscall name resolution uses a built-in table for x86_64 and i386
  architectures (generated from the host kernel headers). Other
  architectures fall back to numeric display.
- The `--input-logs` flag is not yet implemented in modbox v1.
- The `-a`, `-b`, and `--lastreload` flags requiring libaudit are not
  supported in modbox v1 and produce an error.
- Filters apply at event level: an event matches when any of its
  records matches, and matching events print all of their records.

# EXIT STATUS

`0` on success.
`1` on error (unrecognized option, missing input, unsupported flag).

# SEE ALSO

**modbox**(1), **auditd**(8), **ausearch**(8)