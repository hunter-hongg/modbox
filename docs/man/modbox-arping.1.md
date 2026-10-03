% MODBOX-ARPING(1) modbox | User Commands
% modbox project
% 2026-10-03

# NAME

modbox-arping - ping a host by ARP request to resolve its MAC address

# SYNOPSIS

**modbox arping** **-I** *interface* [*OPTION*]... *ip*

# DESCRIPTION

Send ARP REQUEST packets on a local network segment to resolve the MAC
address for a given IPv4 address. On the first matching ARP REPLY the
resolved MAC is printed as `ARP REPLY <ip>: <mac>` and the command exits.

In **-D** (duplicate address detection) mode the probe is sent with sender
IP 0.0.0.0 (an RFC 5227 address-claim probe). Any ARP message whose sender
IP is the probed address counts as a conflict: the tool reports
`<ip> is already in use (<mac>)` and exits 1. When no host answers, it
reports `<ip> is free` and exits 0.

`arping` operates at layer 2 with a raw AF_PACKET socket, so it requires
root or the `CAP_NET_RAW` capability. It is **not** governed by
`net.ipv4.ping_group_range`.

The interface must be given with **-I**; the target is a literal IPv4
address (hostnames are not resolved). Each probe waits up to the **-w**
timeout (default 1 second) for a reply before the next probe is sent.

# OPTIONS

**-I**, **--interface=**<interface>
:   Network interface to use (required). For example `eth0` or `lo`.

**-c**, **--count=**<count>
:   Send at most <count> ARP requests. Without this, arping sends
    indefinitely until a reply is received or it is interrupted.

**-w**, **--timeout=**<seconds>
:   Seconds to wait for a reply after each probe (default 1). Fractional
    values such as 0.2 are accepted; a non-numeric or negative value is a
    usage error.

**-D**, **--dad**
:   Duplicate address detection mode. Probes with sender IP 0.0.0.0 and
    reports whether the address is already in use.

**-q**, **--quiet**
:   On success print only the resolved MAC address (no `ARP REPLY`
    decoration). DAD mode prints nothing on success paths; exit codes
    still distinguish free (0) from conflict (1).

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Display version and exit.

# EXIT STATUS

`0`
:   The target's MAC address was resolved, or (with **-D**) no host
    claimed the probed address.

`1`
:   No ARP reply was received, the interface was not found, the address
    was invalid, the raw socket could not be opened (for example due to
    missing `CAP_NET_RAW`), or (with **-D**) the address is already in
    use.

`2`
:   Usage error (for example missing interface, missing target, or an
    invalid **-w** value).

# NOTES

- IPv4 ARP only (the reference's NDP mode for IPv6 is not implemented).
- Unlike the reference tool, modbox accepts fractional **-w** seconds
  (its **-w** takes integer seconds only).
- The reference's unsolicited-advertisement (`-U`) and advertising (`-A`)
  modes are not implemented.

# SEE ALSO

**modbox-ping**(1), **modbox**(1)
