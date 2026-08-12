% MODBOX-PING(1) modbox | User Commands
% modbox project
% 2026-08-12

# NAME

modbox-ping - send ICMP ECHO_REQUEST to network hosts

# SYNOPSIS

**modbox ping** [*OPTION*]... *destination*

# DESCRIPTION

Send ICMP ECHO_REQUEST packets to a network host and report replies. For
each reply a line is printed with the round-trip time, and a summary of
transmitted, received, packet loss, and (when at least one reply arrived)
RTT min/avg/max/mdev is printed on exit.

The destination may be a literal IPv4 address or a hostname. Hostnames are
resolved via the system resolver to an IPv4 address.

This command uses the Linux *ping socket* interface
(`socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP)`). On most kernels this works
for unprivileged users whose group id falls within
`net.ipv4.ping_group_range`; otherwise root or `CAP_NET_RAW` is required.
The kernel fills in and validates the ICMP header, but the full ICMP echo
header (type, id, sequence) plus payload must be sent, or the kernel
rejects the packet.

# OPTIONS

**-c**, **--count=**<count>
:   Stop after sending <count> ECHO_REQUEST packets rather than running
    until interrupted. A summary is always printed.

**-h**, **--help**
:   Display help and exit.

**--version**
:   Display version and exit.

# EXIT STATUS

`0`
:   At least one ECHO_REPLY was received.

`1`
:   No reply was received, the destination could not be resolved, or the
    socket could not be opened (for example due to missing privileges).

`2`
:   Usage error (for example no destination given or an invalid argument).

# NOTES

- IPv4 only. IPv6 (`-6`) and the interval/size/timeout/quiet/verbose
  tunables are not yet implemented in this tracer-bullet build.
- A SIGINT handler prints the summary on `Ctrl-C`, matching standard ping.

# SEE ALSO

**modbox-arping**(1), **modbox**(1)
