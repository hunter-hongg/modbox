% MODBOX-PING(1) modbox | User Commands
% modbox project
% 2026-10-03

# NAME

modbox-ping - send ICMP ECHO_REQUEST to network hosts

# SYNOPSIS

**modbox ping** [*OPTION*]... *destination*

# DESCRIPTION

Send ICMP ECHO_REQUEST (IPv4) or ICMPv6 Echo Request (IPv6) packets to a
network host and report replies. For each reply a line is printed with the
round-trip time, and a summary of transmitted, received, packet loss, and
(when at least one reply arrived) RTT min/avg/max/mdev is printed on exit.

The destination may be a literal IPv4 or IPv6 address or a hostname. The
address family is taken from the literal address or, for a hostname, from
the resolver; **-4**/**-6** force one family and reject a destination that
does not resolve to it.

This command prefers a raw ICMP/ICMPv6 socket and falls back to the Linux
*ping socket* interface (`SOCK_DGRAM` + `IPPROTO_ICMP`/`IPPROTO_ICMPV6`)
when raw sockets are not permitted. On most kernels the ping socket works
for unprivileged users whose group id falls within
`net.ipv4.ping_group_range`; IPv6 ping sockets require kernel 4.17 or
newer. The kernel fills in and validates the ICMP checksum, but the full
echo header (type, id, sequence) plus payload must be sent, or the kernel
rejects the packet. The reply TTL is requested via `IP_RECVTTL` /
`IPV6_RECVHOPLIMIT` so the `ttl=` field is printed on either socket type.

# OPTIONS

**-c**, **--count=**<count>
:   Stop after sending <count> ECHO_REQUEST packets rather than running
    until interrupted. A summary is always printed.

**-i**, **--interval=**<seconds>
:   Wait <seconds> between probes (default 1). Fractional values such as
    0.2 are accepted. Sending continues on schedule even while replies
    arrive late.

**-s**, **--size=**<bytes>
:   Payload size in bytes (default 56). Must be between 1 and 65507.

**-W**, **--timeout=**<seconds>
:   Per-probe wait timeout in seconds (default 10). After the last probe,
    ping waits at most this long for its reply before giving up.

**-4**, **--ipv4**
:   Force IPv4. A destination that only resolves to IPv6 is a usage error
    (exit 2).

**-6**, **--ipv6**
:   Force IPv6 (ICMPv6 echo). A destination that only resolves to IPv4 is
    a usage error (exit 2).

**-q**, **--quiet**
:   Suppress the per-reply lines; the header line and the final summary
    are still printed.

**-v**, **--verbose**
:   Print which socket type and address family were selected, in addition
    to the normal output.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Display version and exit.

# EXIT STATUS

`0`
:   At least one ECHO_REPLY was received.

`1`
:   No reply was received, the destination could not be resolved, or the
    socket could not be opened (for example due to missing privileges).

`2`
:   Usage error: no destination given, an invalid **-i**/**-s**/**-W**
    value, both **-4** and **-6**, or a forced family that does not match
    the destination.

# NOTES

- Unlike GNU ping, modbox rejects `-s 0` (payload sizes below 1 are usage
  errors), and the summary line keeps modbox's own wording
  (`N packets received`) rather than GNU's `N received`.
- Sequence numbers start at 1, matching the reference.
- A SIGINT handler prints the summary on `Ctrl-C`, matching standard ping.
- Fragment avoidance, source selection (`-I`), routing marks, timestamp
  (`-T`) and preloading (`-p`) options are not implemented.

# SEE ALSO

**modbox-arping**(1), **modbox**(1)
