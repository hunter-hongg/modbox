% MODBOX-HTOP(1) modbox | User Commands
% modbox project
% 2026-08-23

# NAME

modbox-htop - interactive process viewer

# SYNOPSIS

**modbox htop** [*OPTION*]...

# DESCRIPTION

Interactive, real-time process viewer inspired by `htop`.

Displays CPU, memory, swap usage, load average and a sortable process table. The interface is TUI-based and requires a terminal.

Basic navigation:

* Up/Down – move selection
* Space – toggle process
* k – kill selected process
* u – filter by user
* F1 – help
* q – quit

# OPTIONS

`-h`, `--help`
:   Display help and exit.

`--version`
:   Output version information and exit.

No other command-line options are supported; configuration is done interactively.

# EXAMPLES

```bash
modbox htop
```

# NOTES

* Requires a TTY. Running without a terminal prints no output.
* Built with ftxui for the TUI.

# EXIT STATUS

`0` on normal exit, non-zero on error.

# SEE ALSO

**modbox-ps**(1), **modbox-top**(1), **modbox**(1)
