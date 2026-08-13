% MODBOX-LSOF(1) modbox | User Commands
% modbox project
% 2026-08-13

# NAME

modbox-lsof — list open file descriptors

# SYNOPSIS

**modbox lsof** [*OPTION*]... [*PID*]...

# DESCRIPTION

List information about files opened by processes. Reads from the Linux
`/proc` filesystem to enumerate open file descriptors without requiring
special privileges.

If no **PID** is given and **--pid** is not specified, all processes in
`/proc` are scanned.

# OPTIONS

**-p**, **--pid** *PID*
:   Show FDs for the process with the given PID. Multiple **--pid**
    flags or positional PIDs are accepted.

**-c**, **--command** *CMD*
:   Show FDs for processes whose comm name contains *CMD* (case-insensitive
    partial match).

**-u**, **--user** *USER*
:   Show FDs for processes owned by *USER*.

**-t**, **--type** *TYPE*
:   Show only open files of the given type. Accepted values:
    **REG** (regular file), **CHR** (character device), **BLK** (block
    device), **FIFO** or **PIPE** (pipe), **SOCK** or **NET** (socket),
    **DIR** (directory), **MEM** (memory map).

**-d**, **--fd** *FD*
:   Show only the specified file descriptor. *FD* can be a number
    (e.g. `0`, `1`, `2`) or a special token (`cwd`, `rtd`, `txt`,
    `mem`, `log`).

**-i**
:   Show only network files (sockets).

**-n**
:   Do not resolve hostnames for network addresses. IP addresses are
    shown in numeric form.

**-P**
:   Do not resolve port numbers to service names. Numeric port values
    are shown.

**-F**
:   Output in field format (one field per line), suitable for parsing
    by scripts. Fields include `p` (PID), `f` (FD), `n` (name), `a`
    (command), `u` (user), `t` (type), `D` (device), `s` (size/off),
    `i` (inode), and `r` (reference count, with **-R**). Entries are
    separated by `---`.

**-R**
:   Show reference counts (from fdinfo lock entries) in **-F** mode.

**-a**
:   Apply AND logic between all filters. With this flag, an entry must
    satisfy every specified filter to be included in the output.
    Without **-a**, filters are combined with OR logic (an entry matches
    if it satisfies any one filter).

**-r** *N*
:   Repeat output *N* times, with a 1-second delay between iterations.
    Useful for watching FD changes over time.

**-H**, **--heading**
:   Print a header line with column names.

**--json**
:   Output in JSON format. Each entry is a JSON object with fields:
    `pid`, `comm`, `user`, `fd`, `type`, `device`, `size_off`, `node`,
    `name`.

**-h**, **--help**
:   Display help and exit.

**--version**
:   Display version and exit.

# OUTPUT COLUMNS

The default tabular output has the following columns:

COMMAND
:   Process command name (from `/proc/[pid]/comm`).

PID
:   Process ID.

USER
:   Username of the process owner.

FD
:   File descriptor number or special token (`cwd`, `rtd`, `txt`,
    `mem`, `log`).

TYPE
:   File type: **REG** (regular file), **CHR** (character device),
    **BLK** (block device), **PIPE** (pipe), **SOCK** (socket),
    **DIR** (directory), **MEM** (memory mapping), **UNK** (unknown).

DEVICE
:   Mount namespace identifier (from fdinfo `mnt_id`).

SIZE/OFF
:   Current read/write offset (from fdinfo `pos`).

NODE
:   Inode number (from fdinfo `ino`).

NAME
:   Path or resource description. For sockets, the local address is
    resolved from `/proc/net/` when possible (e.g. `127.0.0.1:8080`).

# SPECIAL FD TOKENS

The following special tokens may appear in the FD column:

cwd
:   Current working directory of the process (`/proc/[pid]/cwd`).

rtd
:   Root directory of the process (`/proc/[pid]/root`).

txt
:   Executable text (`/proc/[pid]/exe`).

mem
:   Memory-mapped regions (`[anon]`, `[heap]`, `[stack]`, etc.).

log
:   Log file (a FIFO or socket with a `log:` entry in fdinfo).

# EXAMPLES

```bash
# List all open file descriptors system-wide
modbox lsof

# Show FDs for a specific process
modbox lsof -p 1234

# Show only sockets for a process
modbox lsof -p 1234 -i

# Find all regular files opened by a user
modbox lsof -u root -t regular

# Find sockets for a specific command
modbox lsof -c nginx -t socket

# Show FDs in JSON format for scripting
modbox lsof -p 1234 --json

# Watch a process's FDs change over time
modbox lsof -p 1234 -r 5

# Combined filters with AND logic
modbox lsof -a -u hunter -t socket -i
```

# EXIT STATUS

`0`
:   Success (including when no entries match the filters).

non-zero
:   Invalid option or argument.

# SEE ALSO

**modbox-ps**(1), **modbox**(1)
