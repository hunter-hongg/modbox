% MODBOX-ARPING(1) modbox | User Commands
% modbox project
% 2026-08-12

# NAME

modbox-arping - ping a host by ARP request to resolve its MAC address

# SYNOPSIS

**modbox arping** **-I** *interface* [*OPTION*]... *ip*

# DESCRIPTION

Send ARP REQUEST packets on a local network segment to resolve the MAC
address for a given IPv4 address. On the first matching ARP REPLY the
resolved MAC is printed as `ARP REPLY <ip>: <mac>` and the command exits.

`arping` operates at layer 2 with a raw AF_PACKET socket, so it requires
root or the `CAP_NET_RAW` capability. It is **not** governed by
`net.ipv4.ping_group_range`.

The interface must be given with **-I**; the target is a literal IPv4
address (hostnames are not resolved).

# OPTIONS

**-I**, **--interface=**<interface>
:   Network interface to use (required). For example `eth0` or `lo`.

**-c**, **--count=**<count>
:   Send at most <count> ARP requests. Without this, arping sends
    indefinitely until a reply is received or it is interrupted.

**-h**, **--help**
:   Display help and exit.

**--version**
:   Display version and exit.

# EXIT STATUS

`0`
:   The target's MAC address was resolved.

`1`
:   No ARP reply was received, the interface was not found, the address was
    invalid, or the raw socket could not be opened (for example due to
    missing `CAP_NET_RAW`).

`2`
:   Usage error (for example missing interface, missing target, or an
    invalid argument).

# NOTES

- IPv4 ARP only. Duplicate-address-detection mode (`-D`), quiet mode
  (`-q`), and the timeout tunable are not yet implemented in this
  tracer-bullet build.

# SEE ALSO

**modbox-ping**(1), **modbox**(1)
