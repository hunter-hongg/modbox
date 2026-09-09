% MODBOX-NC(1) modbox | User Commands
% modbox project
% 2026-09-08

# NAME

modbox-nc — Open a TCP/UDP connection and relay stdin/stdout

# SYNOPSIS

**modbox nc** [*OPTIONS*] [HOST] [PORT]

**modbox netcat** [*OPTIONS*] [HOST] [PORT]

# DESCRIPTION

**nc** (netcat) opens a TCP or UDP connection to HOST:PORT and relays
data between the socket and stdin/stdout, acting as a networking Swiss
Army knife for port probing, simple listeners, and interactive messaging.
Supports TCP and UDP, both client and listener modes, with options for
timeout, verbose output, delay between lines, and executing a command
after connection.

Also registered under the alias **netcat**, which is functionally
identical to **nc**.

# OPTIONS

`-c CMD`
:   Execute CMD via `/bin/sh -c` after the connection is established,
    with stdin, stdout, and stderr connected to the socket.

`-d`
:   Set `SO_REUSEADDR` on the socket.

`-i SECS`
:   Insert a delay of SECS seconds between each line read from stdin
    before sending to the socket.

`-k`
:   With `-l`, keep the listener open and accept multiple connections
    in sequence. Without `-k`, the listener exits after the first
    connection closes.

`-l`
:   Listen for an incoming connection instead of connecting out to a
    remote host. The port is given as the (single, numeric) positional
    argument, e.g. **nc -l 8080**; the listener binds on all
    interfaces for the address family. `-p` is ignored in listener
    mode.

`-p PORT`
:   Use PORT as the local/source port when connecting out (the socket
    is bound before connecting). Ignored in listener mode.

`-u`
:   Use UDP (default is TCP).

`-v`
:   Be verbose; print connection status messages to stderr.

`-w SECS`
:   Set connect timeout to SECS seconds. After connecting, socket
    read/write timeouts are also set to SECS.

`-z`
:   Zero-I/O mode: just connect and close, used for port scanning.
    Prints a success line to stdout when the connection succeeds and a
    failure line to stderr when it does not.

`-6`
:   Prefer IPv6 addresses (`AI_V4MAPPED | AI_ADDRCONFIG`), so IPv4-only
    hosts are still reachable via v4-mapped addresses when available.

`-h`, `--help`
:   Display usage information and exit.

`-V`, `--version`
:   Output version information and exit.

# EXAMPLES

Connect to a TCP port and pipe stdin/stdout:

    modbox nc 127.0.0.1 8080

Start a TCP listener on port 9000:

    modbox nc -l 9000

Start a UDP listener:

    modbox nc -u -l 9000

Scan a port (zero-I/O):

    modbox nc -z 127.0.0.1 22

Connect with verbose output and 2-second timeout:

    modbox nc -v -w 2 example.com 80

Send a message via UDP:

    echo "ping" | modbox nc -u 127.0.0.1 9000

Execute a command after connecting (e.g., spawn a shell):

    modbox nc -c '/bin/sh' 127.0.0.1 4444

# EXIT STATUS

`0` on successful connection and clean relay completion, or on
successful `-z` probe.

`1` on connection refused, no routes, resolution failure, or
`-z` probe failure.

`2` on usage errors (unknown option, missing host/port).

# SEE ALSO

**modbox**(1), **modbox-curl**(1), **modbox-wget**(1)
