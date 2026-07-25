# `lf` shell integration via CWD file instead of stdout

`ls --tui` (alias `lf`) is a TUI interactive file browser. When the user quits, the shell should `cd` to the directory they were last browsing. The current design prints the CWD to stdout on exit, relying on the caller to capture it.

That approach is broken: a terminal UI takes over the screen and stdout is unreliable during and after the session (ftxui uses stdout for escape sequences; `printf("\033c")` and `system()` calls race with the shell's capture). Wrapper scripts can't cleanly read the exit path.

We decided to use a file-based protocol: on quit, `lf` writes the final CWD to `~/.cache/lf/cwd`. A shell wrapper (sourced from `lf init <shell>`) reads this file on return and executes `cd`. The TUI never writes to stdout for its own protocol messages.

## Considered Options

- **Stdout capture (status quo).** Simple, `cd "$(modbox lf)"` looks clean. Fails because TUI output corrupts stdout; ftxui and `system()` calls make the terminal state unpredictable on exit.
- **Exit code encoding.** Encode a small path in the exit status. Exit codes max at 255; useless for real paths.
- **File-based protocol (`~/.cache/lf/cwd`).** Slightly more setup (`lf init` must be sourced), but robust against TUI stdout noise and works across any shell. Matches how `ranger` and similar tools handle the same problem.

## Consequences

- `ls_tui` no longer prints CWD to stdout on quit; it only writes the cwd file.
- Users must run `eval "$(modbox lf init zsh)"` (or bash/fish) in their `.zshrc` before `lf` shell integration works.
- `~/.cache/lf/` is created on first use; no global state, no permission issues beyond the standard XDG cache dir.
