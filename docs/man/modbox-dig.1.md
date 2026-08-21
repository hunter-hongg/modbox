% MODBOX-DIG(1) modbox | User Commands
% modbox project
% 2026-08-21

# NAME

modbox-dig - DNS lookup utility

# SYNOPSIS

**modbox dig** [**@server**] [**domain**] [**q-type**] [**q-opt**...]
**modbox dig** **-x** **ip-address** [**d-opt**...]

# DESCRIPTION

Query DNS servers for domain name information. Supports A, AAAA, MX, NS,
CNAME, SOA, PTR, TXT, and SRV record types.

When no options are provided, dig queries for A records by default and
displays a detailed multi-line output format similar to the BIND dig
utility.

# OPTIONS

## Query options

**-t**, **--type=TYPE**
:   Query type: A, AAAA, MX, NS, CNAME, SOA, PTR, TXT, SRV.
    Default is A when domain is specified without type.

**-s**, **--server=SERVER**
:   DNS server to query. If not specified, uses the system DNS server
    from /etc/resolv.conf (or 127.0.0.53 for systemd-resolved).

**-x**, **--reverse=IP**
:   Perform reverse DNS lookup for the given IP address.

## Display options (d-opts)

Options beginning with **+** control output format.

**+short**
:   Display only answer records in short format.

**+noall**
:   Suppress all output sections.

**+answer**
:   Show only the ANSWER section.

**+authority**
:   Show only the AUTHORITY section.

**+comments**
:   Show header and comments (default behavior).

**+noall +comments**
:   Show only header comments.

**+trace**
:   Trace the delegation path from the root servers.

## General options

**-h**, **--help**
:   Display help information and exit.

**-V**, **--version**
:   Output version information and exit.

# OUTPUT FORMATS

## Detailed output (default)

When no **+/short** option is specified, dig outputs:

- **Header**: Command string, response code, ID, flags
- **Question Section**: The query that was sent
- **Answer Section**: DNS resource records returned
- **Authority Section**: NS records for the zone
- **Additional Section**: Extra DNS data (glue records, etc.)
- **Footer**: Server address, timestamp, message size

Example:

```
; <<>> DiG 9.x.x <<>> example.com
;; global options: +cmd
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 12345
;; flags: qr rd ra; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 1

;; QUESTION SECTION:
;example.com.			IN	A

;; ANSWER SECTION:
example.com.		300	IN	A	93.184.216.34

;; Query time: 45 msec
;; SERVER: 127.0.0.53#53(127.0.0.53)
;; WHEN: Mon Aug 21 10:00:00 UTC 2026
;; MSG SIZE  rcvd: 56
```

## Short output (+short)

Only the answer data is printed, one per line:

```
93.184.216.34
```

For MX records, priority and target are shown:

```
10 mail.example.com.
```

# EXAMPLES

```bash
# Basic A record lookup
modbox dig example.com

# Query specific DNS server
modbox dig @8.8.8.8 example.com

# MX record lookup
modbox dig example.com MX

# AAAA record lookup
modbox dig example.com AAAA

# Reverse DNS lookup
modbox dig -x 8.8.8.8

# Short output
modbox dig example.com +short

# TXT record lookup (SPF, DKIM)
modbox dig example.com TXT

# NS record lookup
modbox dig example.com NS
```

# NOTES

- This is a lightweight implementation of dig for modbox.
- DNS query ID is auto-incremented within a single process.
- Response validation checks ID, QR bit, and RCODE.
- For comprehensive DNS debugging, use the system dig if available.

# EXIT STATUS

`0`
:   Success.

`1`
:   DNS error (NXDOMAIN, SERVFAIL, etc.) or query failure.

# SEE ALSO

**nslookup**(1), **host**(1), **dig**(8)
