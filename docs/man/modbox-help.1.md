% MODBOX-HELP(1) modbox | User Commands
% modbox project
% 2026-08-23

# NAME

modbox-help - display help for modbox commands

# SYNOPSIS

**modbox help** [*COMMAND*]...

# DESCRIPTION

Display a list of available modbox commands, or detailed help for a specific command.

When run without arguments, lists all registered commands with short descriptions.

# OPTIONS

None specific; see **modbox** command help for global options.

# EXAMPLES

```bash
modbox help
modbox help cat
modbox <command> --help
```

# NOTES

Detailed help for a command can also be obtained with `<command> --help`.

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

**modbox**(1)
