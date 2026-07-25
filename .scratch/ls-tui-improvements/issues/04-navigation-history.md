# 04 — Add directory navigation history

**What to build:** A directory history stack so the user can move through the filesystem without quitting. `h` and `Backspace` go to the parent directory. `u` jumps back through history; `U` jumps forward. The stack records every `cd` (Enter into a dir, h, u, U) so the user can retrace steps through a deep tree. Quitting (`q`) writes the final CWD to the cwd file instead of stdout.

**Blocked by:** 03

**Status:** ready-for-agent

- [ ] TuiCtx gains history_ vector and history_pos_ index
- [ ] `h` / `Backspace`: cd to parent, push current dir onto history, reload entries
- [ ] `u`: move history_pos_ backward, cd to that directory, reload entries
- [ ] `U`: move history_pos_ forward, cd to that directory, reload entries
- [ ] Enter into a subdirectory clears forward history (standard browser behaviour)
- [ ] History position indicator visible in footer (e.g. "3/7")
- [ ] `q` writes final CWD to ~/.cache/lf/cwd and exits cleanly
