% MODBOX-LF(1) modbox | User Commands
% modbox project
% 2026-08-16

# NAME

modbox-lf - interactive file browser

# SYNOPSIS

**modbox lf** [*OPTION*]...

**modbox lf init** *SHELL*

# DESCRIPTION

**modbox lf** is a thin wrapper around **modbox ls --tui** that provides
an interactive file browser with a two-pane TUI interface.

All options are passed through to **modbox ls --tui**.
There are no additional flags specific to **lf**.

The **lf init** subcommand generates shell function definitions for bash, zsh,
and fish that integrate lf with the current shell session. The generated
function saves the last visited directory and changes to it on subsequent
invocations.

# OPTIONS

The following options are supported when running **modbox lf** directly
(they are forwarded to **ls --tui**):

**-T**, **--layout=LAYOUT**
:   Choose a layout: `two-pane` (default), `one-column`, or `compact`.

**-r**, **--reverse**
:   Reverse the sort order.

**-s**, **--sort=SORT**
:   Sort by: `name`, `size`, `date`, or `ext`. Default: `name`.

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Launch interactive file browser
modbox lf

# Browse a specific directory
modbox lf /home/user/documents

# Generate shell integration for bash
modbox lf init bash

# Generate shell integration for zsh
modbox lf init zsh

# Generate shell integration for fish
modbox lf init fish
```

# NOTES

- **lf** requires a TTY; it will fall back to normal **ls** output if stdout
  is not a terminal.
- The **lf init** output should be sourced from your shell's configuration
  file (e.g., `~/.bashrc`, `~/.zshrc`, or `~/.config/fish/config.fish`).
- Directory history is stored in `$HOME/.cache/lf/cwd`.

# SEE ALSO

**modbox-ls**(1), **modbox-ls_tui**(1), **modbox**(1)
