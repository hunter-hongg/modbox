---
title: modbox-env
section: 1
date: 2026-08-19
author: modbox project
---

# NAME

modbox-env - run a command in a modified environment

# SYNOPSIS

**modbox env** [*OPTION*]... [*NAME*]=[*VALUE*]... [*COMMAND*]

# DESCRIPTION

Set each **NAME** to **VALUE** in the environment and run **COMMAND**.
If no command is specified, print the current environment.

# OPTIONS

**-i**, **--ignore-environment**
:   Start with an empty environment.

**-0**, **--null**
:   End each output line with NUL, not newline.

**-u**, **--unset=NAME**
:   Remove variable from the environment.

**-C**, **--chdir=DIR**
:   Change working directory to DIR.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Set environment variable and run command
modbox env PATH=/usr/bin mycommand

# Start with empty environment
modbox env -i PATH=/usr/bin mycommand

# Unset a variable
modbox env -u HOME mycommand

# Print current environment
modbox env

# Set multiple variables
modbox env FOO=bar BAZ=qux command
```

# EXIT STATUS

`0` on success, non-zero on error. If COMMAND fails, exits with COMMAND's exit status.

# NOTES

- When running a command, all NAME=VALUE assignments are applied before execution.
- Use **-i** to start with a clean environment (useful for security-critical commands).

# SEE ALSO

**modbox-sh**(1), **modbox-bash**(1), **modbox-umask**(1)
