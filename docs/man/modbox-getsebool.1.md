% MODBOX-GETSEBOOL(1) modbox | User Commands
% modbox project
% 2026-10-02

# NAME

modbox-getsebool - report whether SELinux booleans are on or off

# SYNOPSIS

**modbox getsebool** [**-a**] [*BOOLEAN*]...

# DESCRIPTION

Report the current state of SELinux policy booleans. Exactly one of the two
forms is allowed: **-a** reports every boolean, or one or more BOOLEAN names
report just those. modbox getsebool requires SELinux support on the host.

Output follows the system `getsebool` format: `name --> on` for an active
boolean, or `name --> on off` when the pending value differs from the active
value (i.e. a change has been requested but not yet persisted/reloaded).

# OPTIONS

**-a**, **--all**
:   Report the state of all booleans.

**--help**
:   Display help and exit (long option only).

**--version**
:   Output version information and exit.

# EXAMPLES

```bash
# List all booleans
modbox getsebool -a

# Query a single boolean
modbox getsebool httpd_can_network_connect
```

# EXIT STATUS

`0`
:   The booleans were queried successfully.

`1`
:   A usage error, or SELinux is disabled or unsupported.

`255`
:   A named boolean does not exist.

# NOTES

- modbox getsebool requires SELinux support. On a host without SELinux it
  exits non-zero with a message on stderr.
- The `on off` pending form matches the system utility so existing parsers keep
  working.
- Deliberate deviations from the reference (`getsebool` from policycoreutils):
  it has no long options, so it rejects `--help` and prints its usage line
  instead; it stops reporting at the first boolean it cannot query, which is
  why an unknown name exits 255 rather than 1; and it treats a bare `--` as a
  boolean name rather than an end-of-options marker, which argtable3 consumes.

# SEE ALSO

**modbox-setsebool**(1), **modbox-getenforce**(1), **modbox**(1)
