% MODBOX-FUSER(1) modbox | User Commands
% modbox project
% 2026-08-13

# NAME

modbox-fuser — identify processes using files or sockets

# SYNOPSIS

**modbox fuser** [*OPTION*]... *PATH*...
**modbox fuser** [*OPTION*]... -m *MOUNTPOINT*
**modbox fuser** [*OPTION*]... -n *SPACE* *NAME*...

# DESCRIPTION

Show what processes use the specified files, mount points, or network
sockets. Reads from the Linux `/proc` filesystem without requiring special
privileges. Supports path, mount, file descriptor, network socket, and
Unix domain socket lookup modes.

# OPTIONS

**-v**, **--verbose**
:   Print the PID, username, access mode, and path of each process on its
    own line. Without **-v** the default output prints space-separated PID
    and access mode tokens in a single row.

**-u**, **--user**
:   Append the username in parentheses to each entry. (Included by default
    in verbose mode.)

**-p**, **--pid**
:   Print PID values only, one per line. Equivalent to **-t**.

**-t**
:   Print PID values only, one per line.

**-m**, **--mount**, **-c**
:   Show processes using files on the specified mount point or filesystem.
    The mount is identified by matching the device ID of files under the
    given path. **-c** is an alias for **-m**.

**-s**, **--silent**, **-f**
:   Silent mode. Print nothing. Exit `0` if no process is found, `1` if at
    least one process is found.

**-k**, **--kill**
:   Send a signal to all processes using the specified resource. The
    default signal is **SIGKILL**. A signal is selected with the standard
    `-SIGNAL` (e.g. **-SIGTERM**) or `-N` (e.g. **-15**) syntax. Combining
    **-k** with **-v** shows which processes would be killed without
    actually doing so (dry-run).

**-i**, **--interactive**
:   Interactive kill mode. Prompt before killing each process. Requires
    **-k**.

**-l**, **--list-signals**
:   List supported signal names and exit.

**-n** *SPACE*
:   Interpret the argument as a name in the given namespace rather than a
    file path. Supported values: **tcp**, **udp**, **unix**, **fd**,
    **block**. For TCP and UDP the name is a port number; for Unix sockets
    it is a path; for **fd** it is a file descriptor number; for **block**
    it is a device name.

**-a**, **--all**
:   Include processes that use the path via cwd/root (all access modes).

**-z**, **--zombie**
:   Include zombie processes.

**-r**, **--recursive**
:   Recurse into directory paths.

**-R**, **--with-mounts**
:   With **-m**, recursively descend into mounts.

**-M**
:   Never report processes that use a lower-level mount.

**-w**, **--write**
:   Match processes with write access only (access `>` without `<`).

**-4**
:   Restrict network matching to IPv4 addresses only.

**-6**
:   Restrict network matching to IPv6 addresses only.

**--json**
:   Output results as a JSON array. Each entry contains the fields
    `pid`, `access`, `path`, `fd`, `comm`, `user`.

**-h**, **--help**
:   Display help and exit.

**--version**
:   Display version and exit.

# ACCESS MODES

Access modes are read from `/proc/[pid]/fdinfo/<fd>` flags:

`<`
:   Opened read-only (`O_RDONLY`).

`>`
:   Opened write-only (`O_WRONLY`).

`<>`
:   Opened read-write (`O_RDWR`).

`a`
:   Has `O_APPEND` set.

`c`
:   Has `O_CREAT` set.

Multiple access tokens may appear together.

# KILL MODE

When **-k** is specified without a signal the default signal is
**SIGKILL** (`9`). A signal is selected with the standard `-SIGNAL`
(e.g. **-SIGTERM**) or `-N` (e.g. **-15**) syntax.

# EXAMPLES

```bash
# Show processes using /var/log/syslog
modbox fuser /var/log/syslog

# Verbose output with usernames
modbox fuser -v /var/log/syslog

# Find processes on the /home mount point
modbox fuser -m /home

# Find processes using TCP port 22
modbox fuser -n tcp 22

# Find processes using a Unix socket
modbox fuser -n unix /run/dbus/system_bus_socket

# Kill processes using a file with SIGTERM
modbox fuser -k -SIGTERM /tmp/lockfile

# Interactive kill with confirmation
modbox fuser -k -SIGTERM -i /tmp/lockfile

# Silent mode for scripting
if modbox fuser -s /var/lock/mutex; then
    echo "resource is free"
fi

# JSON output for scripting
modbox fuser --json /var/log/syslog

# List zombie processes using a file
modbox fuser -z /var/log/syslog

# Show all processes including zombies
modbox fuser -a /var/log/syslog
```

# EXIT STATUS

`0`
:   No processes found using the specified resource (or at least one
    process found in silent mode **-s**).

`1`
:   At least one process was found (or no processes found in silent mode
    **-s**).

non-zero
:   Invalid option or argument.

# SEE ALSO

**modbox-lsof**(1), **modbox-ps**(1), **modbox**(1)
