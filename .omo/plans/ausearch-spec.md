# Spec: ausearch Command

## Problem Statement

Linux system administrators and security engineers need to query the audit log (produced by the kernel's audit subsystem and written by `auditd` to `/var/log/audit/audit.log`) to investigate security events, trace user activity, and debug SELinux denials. The standard tool `ausearch` is part of the `audit` userspace package and provides rich filtering capabilities, but modbox currently lacks this command. Users of modbox as a minimal multi-call binary cannot search audit logs without installing the full `audit` package separately.

## Solution

Implement the `ausearch` command as a new modbox subcommand that:
- Reads and parses the Linux audit log file format (key=value records)
- Supports the most commonly used filter options (`-m`, `-ui`, `-ul`, `-f`, `-k`, `-ts`, `-te`, `-sc`, `-c`, `-x`, `-p`, `-i`, `-r`)
- Handles multi-record event assembly (records sharing the same `msg=audit(epoch:serial)` stamp)
- Supports stdin pipe input and `--input FILE` for flexibility
- Provides default and raw output formatting
- Gracefully degrades when libaudit is unavailable (same pattern as existing `audit2allow`)

## User Stories

1. As a system administrator, I want to search audit logs by record type (e.g., `-m SYSCALL`, `-m AVC`), so that I can quickly find specific kinds of security events.
2. As a security engineer, I want to filter audit events by user ID (`-ui`), login UID (`-ul`), or any UID field (`-ua`), so that I can trace all activity associated with a particular user account.
3. As a sysadmin, I want to search by filename (`-f`), so that I can find all events involving a specific file path (e.g., `/etc/shadow`).
4. As a security analyst, I want to search by audit rule key (`-k`), so that I can find events tagged with a specific rule label.
5. As an incident responder, I want to filter by time range (`--start`, `--end`), so that I can narrow down events to a specific window during an investigation.
6. As a developer, I want to support special time keywords (`today`, `yesterday`, `recent`, `this-hour`, `boot`), so that common investigative queries are quick to write.
7. As a system administrator, I want to search by syscall name or number (`-sc`), so that I can find all executions of a particular system call (e.g., `execve`, `open`).
8. As a security engineer, I want to filter by syscall exit code (`-e`), so that I can find failed system calls (negative errno values).
9. As a sysadmin, I want to search by process ID (`-p`) or parent PID (`-pp`), so that I can trace activity of a specific process or its children.
10. As a security analyst, I want to search by command name (`-c`), so that I can find all events involving a specific executable.
11. As a developer, I want to search by full executable path (`-x`), so that I can find events for a specific binary regardless of how it was invoked.
12. As a system administrator, I want interpret mode (`-i`) that converts numeric UIDs to usernames and syscall numbers to names, so that output is human-readable.
13. As a security engineer, I want raw output mode (`-r`, `--raw`), so that I can pipe audit data to other tools like `aureport` without formatting interference.
14. As a sysadmin, I want to read audit logs from a file (`--input` or `-i`), so that I can analyze archived or remote audit logs without root access to the live log.
15. As a developer, I want to pipe audit log data via stdin, so that I can compose commands like `cat audit.log | modbox ausearch -m AVC`.
16. As a security analyst, I want to filter by SELinux subject context (`-su`), so that I can find events involving a specific SELinux domain.
17. As a sysadmin, I want to filter by SELinux object context (`-o`), so that I can find access attempts to files with a specific security context.
18. As a developer, I want whole-word matching (`--word`), so that filename searches don't match partial strings (e.g., `passwd` shouldn't match `password`).
19. As a security engineer, I want to stop after the first matching event (`--just-one`), so that I can quickly verify a hypothesis without scrolling through long output.
20. As a sysadmin, I want to search by group ID (`-gi`, `-ge`, `-ga`), so that I can trace group-level activity.
21. As a developer, I want to filter by architecture (`--arch`), so that I can distinguish 32-bit vs 64-bit syscall events.
22. As a security analyst, I want to filter by success/failure (`-sv yes/no`), so that I can focus on denied or erroneous operations.
23. As a sysadmin, I want line-buffered output (`-l`), so that piped output flushes immediately for real-time monitoring.
24. As a developer, I want the `--help` flag to display usage information, so that I can learn the available options.
25. As a sysadmin, I want the `--version` flag to print version info, so that I can verify which modbox build I'm running.
26. As a security engineer, I want invalid options to produce an error on stderr and a non-zero exit code, so that I catch typos early.
27. As a developer, I want the command to run without a terminal (work with pipes), so that it fits into automated scripts and CI pipelines.
28. As a sysadmin, I want multi-record events to be assembled correctly (records with the same `msg=audit()` stamp grouped together), so that I see complete syscall + PATH + PROCTITLE sequences.
29. As a developer, I want the command to accept audit log data from stdin when no file is specified and stdin is not a terminal, so that piping works naturally.
30. As a security analyst, I want to search by login session ID (`--session`), so that I can trace all activity within a user's login session.
31. As a sysadmin, I want to search by terminal (`-tm`), so that I can find events from a specific TTY (e.g., `pts/0`).
32. As a developer, I want unknown message types listed when `-m` is used without arguments, so that I can discover valid filter types.
33. As a security engineer, I want CSV output format (`--format csv`), so that I can import audit data into spreadsheets or analysis tools.
34. As a sysadmin, I want the command to work without libaudit installed (read log files directly), so that modbox remains usable in minimal environments.
35. As a developer, I want the command to report a clear error when the audit log file is not readable, so that permission issues are diagnosable.

## Implementation Decisions

### Module Structure

The command follows the modbox standard three-file pattern:
- Header: `include/commands/ausearch.hpp` — declares `int ausearch_command(int argc, char** argv)`
- Implementation: `src/commands/ausearch.cpp` — full implementation with `REGISTER_COMMAND("ausearch", ...)`
- Test: `tests/test_ausearch.sh` — bash test suite
- Man page: `docs/man/modbox-ausearch.1.md` — added to `Makefile MAN_SOURCES`

No other existing files need modification (Makefile auto-discovers new `.cpp` via `find`).

### Data Model

**AuditRecord** — a single line's parsed key=value fields:
- `type`: record type string (e.g., "SYSCALL", "AVC", "PATH")
- `epoch`: uint64_t seconds from `msg=audit(EPOCH.MSEC:SERIAL)`
- `serial`: uint32_t event serial number
- `fields`: `std::unordered_map<std::string, std::string>` of all key=value pairs

**AuditEvent** — a reassembled multi-record event:
- `epoch`, `serial`: the event's timestamp and serial
- `records`: `std::vector<AuditRecord>` in order of appearance
- Event completion heuristics: PROCTITLE/AUDIT_EOE records end an event; a 2-second time gap also triggers completion

### Parsing Approach

- Parse each line as `key=value` pairs using simple string scanning (no regex for the main loop — regex only for optional filters)
- `msg=audit(EPOCH.MSEC:SERIAL)` is the event join key — all records with the same stamp belong to one event
- Syscall argument fields (`a0`–`a3`) are hex-encoded; interpret mode resolves them using `ausyscall` database or local syscall tables
- UID/GID resolution uses `getpwuid()` / `getgrgid()` from `<pwd.h>` / `<grp.h>`

### Filter Semantics

- All top-level filters combine with AND logic (an event must satisfy all specified filters)
- Multiple `-m` (message type) options combine with OR within the type category
- Multiple `-n` (node) options combine with OR within the node category
- Empty filter set = return all events (subject to input source)

### Time Parsing

- Support special keywords: `now`, `recent` (10 min ago), `this-hour`, `boot`, `today`, `yesterday`, `this-week`, `week-ago`, `this-month`, `this-year`
- Support explicit date/time strings in locale-dependent format (use `strptime` with `%x %X`)
- Store start/end as `time_t` for comparison against event epochs

### libaudit Dependency Strategy

Following the established pattern from `audit2allow`:
- Do NOT add `libaudit` to Makefile `PKGS` (it's not available on the build system)
- Read audit log files directly using `std::ifstream` (same approach as `audit2allow`'s `-i` and stdin paths)
- Flags that would require libaudit (`-a` for all events, `-b` for boot, `-l` for last reload) produce a clear stderr error and exit 1
- All file/stdin-based filtering works without libaudit

### Output Formats (Phase 1)

- **Default**: Timestamp separator line (`---- time->...`) followed by formatted records
- **Interpret (`-i`)**: Same as default but with UID→name, syscall num→name, hex→ASCII conversion
- **Raw (`-r`, `--raw`)**: Unformatted key=value lines, pipe-friendly
- CSV, text, and escape modes are deferred (see Out of Scope)

### Exit Codes

- `0`: Success (matches found, or `--help`/`--version`)
- `1`: No matches, argument error, or file read error
- `10-12`: Checkpoint-related errors (deferred — not implemented in Phase 1)

### Option Set (Phase 1)

Implemented in Phase 1:
- `-a`/`--event`, `-m`/`--message`, `-n`/`--node`
- `-ui`/`--uid`, `-ue`/`--uid-effective`, `-ua`/`--uid-all`, `-ul`/`--loginuid`
- `-gi`/`--gid`, `-ge`/`--gid-effective`, `-ga`/`--gid-all`
- `-p`/`--pid`, `-pp`/`--ppid`, `-c`/`--comm`, `-x`/`--executable`
- `-sc`/`--syscall`, `-e`/`--exit`, `--arch`
- `-f`/`--file`, `-k`/`--key`
- `-su`/`--subject`, `-o`/`--object`, `-se`/`--context`
- `-ts`/`--start`, `-te`/`--end`
- `-i`/`--interpret`, `-r`/`--raw`, `--format` (raw/default/interpret only)
- `-l`/`--line-buffered`, `--just-one`, `--word`
- `--input`/`-if`, `--input-logs`
- `-hn`/`--host`, `-tm`/`--terminal`, `-sv`/`--success`
- `-v`/`--version`, `-h`/`--help`

Not implemented in Phase 1 (clearly marked as unsupported, similar to audit2allow's approach):
- `--checkpoint`, `--eoe-timeout`, `--debug`, `--escape`, `--session`, `--uuid`, `--vm-name`
- `--format csv`, `--format text`, `--extra-keys`, `--extra-labels`, `--extra-obj2`, `--extra-time`

## Testing Decisions

### Test Strategy

Tests follow the existing modbox test framework (`tests/framework.sh`):
- `assert_cmd`, `assert_cmd_pat`, `assert_cmd_not_pat`, `assert_cmd_pat_stderr`
- Tests run in subprocesses (via `$()` capture) for isolation
- Input is provided via heredocs piped to stdin or written to `$TMPDIR` files

### What Makes a Good Test

- Test external behavior (stdout/stderr/exit code), not internal parsing logic
- Use synthetic audit log data that mimics real `msg=audit()` format
- Cover both happy path and error path for each major feature
- Tests must pass without root or a real audit log (all tests use synthetic data)

### Test Modules (priority order)

1. **Help & version**: `--help` outputs usage, `--version` outputs version string, both exit 0
2. **Error handling**: unknown option → stderr error + non-zero exit; conflicting input sources → error
3. **No input on terminal**: when stdin is a terminal with no `--input`, print error and exit 1
4. **Empty input**: piped empty input → no output, exit 0
5. **Event assembly**: multi-record event (SYSCALL + PATH + PROCTITLE with same msg stamp) is output as a single grouped event
6. **Message type filter** (`-m`): filter by `SYSCALL` returns only SYSCALL-type records; filter by `AVC` returns only AVC records
7. **User ID filter** (`-ui`): filter by uid finds matching events; non-matching uid returns nothing
8. **Login UID filter** (`-ul`): filter by auid finds matching events
9. **Filename filter** (`-f`): filter by path finds events with matching PATH records
10. **Key filter** (`-k`): filter by audit rule key finds matching events
11. **Syscall filter** (`-sc`): filter by syscall name finds matching SYSCALL records
12. **Time filter** (`-ts`/`-te`): events within range are included; events outside are excluded
13. **Interpret mode** (`-i`): numeric UIDs resolved to names in output; raw mode shows numbers
14. **Raw mode** (`-r`): output is unformatted key=value lines
15. **Stdin pipe**: piped audit log data is processed correctly
16. **Input file** (`--input`): file-based input works identically to stdin
17. **Missing file**: non-existent input file → stderr error, exit 1
18. **Libaudit-unsupported flags**: `-a` (all events) → clear error message
19. **Whole-word filter** (`--word`): partial match does not match; whole match does
20. **Success filter** (`-sv`): `yes` matches successful syscalls; `no` matches failed ones

### Prior Art

- `tests/test_audit2allow.sh` — most similar: synthetic audit-style input, pipe-based testing, multi-record event concepts
- `tests/test_chcon.sh` — simple error-path testing pattern
- `tests/test_getfacl.sh` — setup + conditional feature tests

### Test Data Format

Use synthetic audit log lines matching the real format:
```
type=SYSCALL msg=audit(1717056137.482:90412): arch=c000003e syscall=2 success=yes exit=0 a0=... uid=0 euid=0 comm="cat" exe="/usr/bin/cat"
type=PATH msg=audit(1717056137.482:90412): item=0 name="/etc/passwd" inode=12345 dev=fd:00 mode=0100644
type=PROCTITLE msg=audit(1717056137.482:90412): proctitle=636174002F6574632F706173737764
```

## Out of Scope

- **libaudit integration**: no netlink socket access; no reading from the kernel audit stream directly
- **Checkpoint support**: no incremental search state persistence (`--checkpoint`)
- **CSV/text output formats**: only default, interpret, and raw formats in Phase 1
- **Escape modes**: `--escape tty/shell/shell_quote` not implemented
- **UUID/VM filters**: `-uu`/`--uuid`, `-vm`/`--vm-name` not implemented
- **AVC policy generation**: that is `audit2allow`'s job; ausearch only searches
- **`-a`/`-b`/`-l` flags**: require libaudit, produce clear error messages
- **Real filesystem audit log**: tests use synthetic data; no requirement for `/var/log/audit/audit.log` to exist

## Further Notes

- The existing `audit2allow` command already demonstrates the pattern for audit-log-adjacent commands in modbox. `ausearch` should follow the same conventions for argtable3 usage, options struct pattern, and error handling.
- `ausearch` is a read-only command (no filesystem modifications), making it inherently safe.
- The command will bring modbox's total from 137 to 138 registered commands.
- Per ADR-014, a man page at `docs/man/modbox-ausearch.1.md` must be added and listed in `Makefile MAN_SOURCES`.
