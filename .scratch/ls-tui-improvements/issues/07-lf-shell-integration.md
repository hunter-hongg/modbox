# 07 — Add lf shell integration command and CWD file protocol

**What to build:** A new `lf` command module that implements `lf init <shell>` — it prints a shell function to stdout that reads `~/.cache/lf/cwd` and executes `cd` before invoking `modbox ls --tui`. The `ls_tui` quit path is updated to write the final directory to that file (creating `~/.cache/lf/` if needed) instead of printing it to stdout. Supported shells: zsh, bash, fish.

**Blocked by:** None — can start immediately (parallel to 02–06).

**Status:** ready-for-agent

- [ ] New lf command module with init subcommand
- [ ] `lf init zsh` prints a zsh function that reads the cwd file and cd's
- [ ] `lf init bash` prints a bash function with the same behaviour
- [ ] `lf init fish` prints a fish function with the same behaviour
- [ ] `ls_tui_command` quit path: write current_dir to ~/.cache/lf/cwd (create dir if missing)
- [ ] `ls_tui_command` no longer prints CWD to stdout on quit
- [ ] lf is registered as an alias for ls --tui in the command registry
- [ ] README documents `eval "$(modbox lf init zsh)"` setup
