---
title: modbox-hostname
section: 1
date: 2026-08-19
author: modbox project
---

# NAME

modbox-hostname - show or set the system hostname

# SYNOPSIS

**modbox hostname** [**OPTION**]... [**NEWNAME**]

# DESCRIPTION

Display or set the hostname of the system.
Without arguments and without options, displays the current hostname.
With a **NEWNAME** argument and appropriate privileges, sets the hostname.

# OPTIONS

**-i**, **--ip-address**
:   Print the IP addresses for the hostname.

**-f**, **--fqdn**, **--long**
:   Print the fully qualified domain name.

**-d**, **--domain**
:   Print the domain name part of the FQDN.

**-s**, **--short**
:   Print the short hostname (first label of FQDN).

**-a**, **--address**
:   Alias for **-i**.

**-A**, **--all-fqdn**
:   Print all fully qualified domain names.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Show current hostname
modbox hostname

# Show FQDN
modbox hostname -f

# Show short hostname
modbox hostname -s

# Show IP addresses
modbox hostname -i

# Set hostname (requires root)
sudo modbox hostname newhost.example.com
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- Setting the hostname requires root privileges.
- Changes are not persistent across reboots unless saved in system configuration.
- The FQDN consists of the short hostname plus the domain name.

# SEE ALSO

**modbox-uname**(1), **modbox-ip**(1), **modbox-ifconfig**(1)
