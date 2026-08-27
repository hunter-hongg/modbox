% MODBOX-TCPDUMP(1) modbox | User Commands
% modbox project
% 2026-08-27

# NAME

modbox-tcpdump - capture and display network packets

# SYNOPSIS

**modbox tcpdump** [**OPTION**]... [**EXPR**]

# DESCRIPTION

Capture network packets and display information about them. Packets can be
read live from a network interface or from a previously saved pcap file, and
raw packets can be written to a file for later analysis.

When a capture filter expression **EXPR** is given, only packets matching the
filter are processed (see **FILTER SYNTAX** below).

# OPTIONS

**-r** *file*
:   Read packets from *file* in pcap format instead of capturing live
    traffic.

**-w** *file*
:   Write raw captured packets to *file* in pcap format rather than
    displaying them.

**-c** *count*
:   Exit after receiving *count* packets.

**-i** *interface*
:   Listen for packets on *interface*.

**-s** *snaplen*
:   Capture *snaplen* bytes of each packet. Default is 262144.

**-e**
:   Print the link-level header on each line.

**-q**
:   Quiet output (print less protocol information).

**-v**
:   Verbose output (print more protocol information).

**-x**
:   Print a hex dump of each packet.

**-n**
:   Do not convert addresses to names. Accepted for compatibility as a
    no-op.

**-nn**
:   Do not convert protocol numbers to names either. Accepted for
    compatibility as a no-op.

**-tt**
:   Print unformatted (epoch) timestamps.

**-f** *expr*
:   Set the capture filter expression.

**-h**, **--help**
:   Display help information and exit.

**-V**, **--version**
:   Output version information and exit.

# FILTER SYNTAX

Capture filters select which packets are processed. The grammar is:

```
expr   := term {"or" term}
term   := factor {"and" factor}
factor := "not" factor | "(" expr ")" | atom
atom   := host | net | port | proto | src | dst
```

Protocol shorthands:

**tcp**, **udp**, **icmp**, **arp**, **ip**, **ipv6**, **icmp6**

# EXAMPLES

```bash
# Capture all traffic on interface eth0
modbox tcpdump -i eth0

# Capture HTTP traffic on port 80
modbox tcpdump -i eth0 port 80

# Read a dump file and filter for TCP packets
modbox tcpdump -r capture.pcap tcp
```

# EXIT STATUS

`0`
:   Success.

`1`
:   Runtime error (for example, opening an interface or pcap file failed).

`2`
:   Usage or filter parse error.

# NOTES

- Filtering is evaluated in user space after decoding, not in the kernel.
- Promiscuous mode is not enabled; only packets addressed to the host on
  the selected interface are captured.
- **-n** and **-nn** are accepted as no-ops for compatibility.

# SEE ALSO

**dig**(1), **nslookup**(1)
