% MODBOX-TC(1) modbox | User Commands
% modbox project
% 2026-08-31

# NAME

modbox-tc - Show traffic control (qdisc/class/filter) state

# SYNOPSIS

**modbox tc** [*OPTIONS*] { **qdisc** | **class** | **filter** } *show* [*dev* NAME]

# DESCRIPTION

Show traffic control configuration on the host. This is the read-only,
introspection-only subset of the standard `tc` utility: it lists the queuing
disciplines (qdiscs), classes, and filters already configured in the kernel. It
does **not** add, delete, or modify any traffic-control state, and requires no
special privileges to read the kernel's configuration.

# OBJECTS

- **qdisc** — queuing disciplines attached to interfaces.
- **class** — traffic classes within a qdisc hierarchy.
- **filter** — packet classification rules.

# OPTIONS

`-s`, `--stats`
:   Print per-object statistics (bytes, packets, drops, overlimits, requeues, backlog).

`-d`, `--details`
:   Print detailed parameters of each object.

`-json`
:   Emit JSON instead of text. The JSON carries the same fields as the text view (kind, handle, dev, parent, refcnt) and the stats block when `-s` is given.

`-pretty`
:   Indent the JSON output (only meaningful with `-json`).

`-h`, `--help`
:   Display help and exit.

`--version`
:   Output version information and exit.

# EXAMPLES

Show every qdisc on the host:

```
modbox tc qdisc show
```

Show only the qdisc attached to `eth0`:

```
modbox tc qdisc show dev eth0
```

Show qdiscs with statistics, in JSON:

```
modbox tc -s -json qdisc show
```

# EXIT STATUS

`0` on success, non-zero on error (e.g. unknown object, unknown device).

# SEE ALSO

**modbox-ip**(1), **modbox-ss**(1)
