---
title: modbox-id
section: 1
date: 2026-08-19
author: modbox project
---

# NAME

modbox-id - print user and group information

# SYNOPSIS

**modbox id** [**OPTION**]... [**USER**]

# DESCRIPTION

Print user and group information for the specified **USER**, or the current user if none specified.

# OPTIONS

**-u**, **--user**
:   Print only the effective user ID.

**-g**, **--group**
:   Print only the effective group ID.

**-G**, **--groups**
:   Print all group IDs.

**-n**, **--name**
:   Print a name instead of a number.

**-r**, **--real**
:   Print the real ID instead of the effective ID.

**-z**, **--zero**
:   Delimit output with NUL, not whitespace.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Show current user info
modbox id

# Show only user ID
modbox id -u

# Show only group ID
modbox id -g

# Show all group IDs
modbox id -G

# Show names instead of numbers
modbox id -un

# Show info for specific user
modbox id root
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- By default, all IDs (user and groups) are printed.
- Use **-n** with **-u**, **-g**, or **-G** to get names instead of numbers.

# SEE ALSO

**modbox-whoami**(1), **modbox-uname**(1), **modbox-groups**(1)
