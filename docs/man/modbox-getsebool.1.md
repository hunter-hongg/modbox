% MODBOX-GETSEBOOL(1) modbox | User Commands
% modbox project
% 2026-08-29

# NAME

modbox-getsebool - report whether SELinux booleans are on or off

# SYNOPSIS

**modbox getsebool** [*OPTION*]... [*BOOLEAN*]...

# DESCRIPTION

Report the current state of SELinux policy booleans. With no arguments, list
every boolean. With one or more BOOLEAN names, report only those. modbox
getsebool requires SELinux support on the host.

Output follows the system `getsebool` format: `name --> on` for an active
boolean, or `name --> on off` when the pending value differs from the active
value (i.e. a change has been requested but not yet persisted/reloaded).

# OPTIONS

**--help**
:   Display help and exit (long option only).

**--version**
:   Output version information and exit.

# EXAMPLES

```bash
# List all booleans
modbox getsebool

# Query a single boolean
modbox getsebool httpd_can_network_connect
```

# EXIT STATUS

`0`
:   The booleans were queried successfully.

`1`
:   An error occurred (e.g. SELinux is disabled or unsupported, or a named
    boolean does not exist).

# NOTES

- modbox getsebool requires SELinux support. On a host without SELinux it
  exits non-zero with a message on stderr.
- The `on off` pending form matches the system utility so existing parsers keep
  working.

# SEE ALSO

**modbox-setsebool**(1), **modbox-getenforce**(1), **modbox**(1)
