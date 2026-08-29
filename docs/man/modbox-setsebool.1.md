% MODBOX-SETSEBOOL(1) modbox | User Commands
% modbox project
% 2026-08-29

# NAME

modbox-setsebool - set SELinux booleans at runtime

# SYNOPSIS

**modbox setsebool** [*OPTION*]... *BOOLEAN*... *on*|*off*

**modbox setsebool** [*OPTION*]... *BOOLEAN*=*on*|*off*...

# DESCRIPTION

Set SELinux policy booleans. Two invocation forms are supported:

1. **Legacy form**: `setsebool [-P] name on|off` toggles a single boolean.
2. **Batch form**: `setsebool [-P] name1=on name2=off ...` sets several booleans
   in one transaction.

Without `-P` the change applies at runtime only and is lost on reboot. With
`-P` the change is also committed to the on-disk policy so it persists.

modbox setsebool requires SELinux support on the host; persisting (`-P`)
requires sufficient privilege.

# OPTIONS

**-P**, **--persistent**
:   Set the permanent fact in the policy (persist across reboot).

**--help**
:   Display help and exit (long option only).

**--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Toggle one boolean at runtime
modbox setsebool httpd_can_network_connect on

# Persist a boolean across reboot
modbox setsebool -P httpd_can_network_connect on

# Set several booleans in one transaction
modbox setsebool ftp_home_dir=on tftp_anon_write=off
```

# EXIT STATUS

`0`
:   The boolean(s) were set successfully.

`1`
:   An error occurred (e.g. invalid invocation, unknown boolean, invalid value,
    or insufficient privilege).

# NOTES

- modbox setsebool requires SELinux support. On a host without SELinux, or
  without privilege to set/persist, the command exits non-zero with a message
  on stderr and does not modify the host policy.
- Accepted values: `on`/`off`, `1`/`0`, `true`/`false`.

# SEE ALSO

**modbox-getsebool**(1), **modbox-getenforce**(1), **modbox**(1)
