% MODBOX-HOSTID(1) modbox | User Commands
% modbox project
% 2026-08-23

# NAME

modbox-hostid - print numeric identifier for current host

# SYNOPSIS

**modbox hostid** [*OPTION*]...

# DESCRIPTION

Print the numeric identifier of the current host in hexadecimal (8 digits).

The identifier is obtained from the system's `gethostid()` call.

# OPTIONS

`-h`, `--help`
:   Display help and exit.

`--version`
:   Output version information and exit.

# EXAMPLES

```bash
modbox hostid
echo "Host ID: $(modbox hostid)"
```

# NOTES

Output is always 8 lower-case hexadecimal digits, zero-padded.

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

**modbox-uname**(1), **modbox**(1)
