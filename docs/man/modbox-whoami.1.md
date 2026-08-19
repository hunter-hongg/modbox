---
title: modbox-whoami
section: 1
date: 2026-08-19
author: modbox project
---

# NAME

modbox-whoami - print the effective user name

# SYNOPSIS

**modbox whoami** [**OPTION**]...

# DESCRIPTION

Print the username associated with the current effective user ID.
This is equivalent to running `id -un`.

# OPTIONS

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Print current username
modbox whoami

# Use in scripts to check user
if [ "$(modbox whoami)" = "root" ]; then
    echo "Running as root"
fi
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- This command returns the effective user name, not necessarily the login name.
- Equivalent to `id -un` or `logname` (when run from a login shell).

# SEE ALSO

**modbox-id**(1), **modbox-uname**(1), **modbox-logname**(1)
