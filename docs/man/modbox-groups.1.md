% MODBOX-GROUPS(1) modbox | User Commands
% modbox project
% 2026-08-23

# NAME

modbox-groups - print group memberships

# SYNOPSIS

**modbox groups** [*OPTION*]... [*USERNAME*]...

# DESCRIPTION

Print group memberships for each USERNAME, or for the current process if no user is specified.

When USERNAME is omitted, the real user ID of the current process is used.

# OPTIONS

`-h`, `--help`
:   Display help and exit.

`--version`
:   Output version information and exit.

# EXAMPLES

```bash
modbox groups
modbox groups alice
modbox groups alice bob
```

# NOTES

Group names are resolved via `getgrgid`. If the name cannot be resolved, the numeric GID is printed.

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

**modbox-id**(1), **modbox-whoami**(1), **modbox**(1)
