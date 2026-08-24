% MODBOX-GETENFORCE(1) modbox | User Commands
% modbox project
% 2026-08-23

# NAME

modbox-getenforce - print the current SELinux mode

# SYNOPSIS

**modbox getenforce** [*OPTION*]...

# DESCRIPTION

Print the current SELinux enforcement mode.

Output is one of `Enforcing`, `Permissive`, or `Disabled`.

# OPTIONS

`-h`, `--help`
:   Display help and exit.

`--version`
:   Output version information and exit.

# EXAMPLES

```bash
modbox getenforce
if modbox getenforce | grep -q Enforcing; then
  echo "SELinux is enforcing"
fi
```

# NOTES

Requires the SELinux library; on systems without SELinux support, the command prints `Disabled`.

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

**modbox-getfacl**(1), **modbox-setfacl**(1), **modbox**(1)
