% MODBOX-NETCAT(1) modbox | User Commands
% modbox project
% 2026-09-09

# NAME

modbox-netcat — Open a TCP/UDP connection and relay stdin/stdout
(**netcat** alias of **nc**)

# SYNOPSIS

**modbox netcat** [*OPTIONS*] [HOST] [PORT]

# DESCRIPTION

**netcat** is a registered alias of **nc**: it opens a TCP or UDP
connection to HOST:PORT and relays data between the socket and
stdin/stdout. It is functionally identical to **nc**; see
**modbox-nc**(1) for the full option and behavior reference.

# OPTIONS

See **modbox-nc**(1) — the exact same options apply.

# SEE ALSO

**modbox**(1), **modbox-nc**(1)
