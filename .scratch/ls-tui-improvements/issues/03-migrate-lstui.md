# 03 — Migrate ls_tui to TuiBase

**What to build:** LsfComponent inherits from TuiBase and implements the three virtual hooks. After migration, ls --tui gains proper scroll offset management, alternating row backgrounds, and a consistent footer for free. The two-pane layout (entry list + preview pane) is preserved. This is the foundation ticket — every subsequent ls_tui improvement depends on it.

**Blocked by:** 01

**Status:** ready-for-agent

- [ ] LsfComponent inherits TuiBase, implements fill_entries/render_row/on_command_key
- [ ] Two-pane layout (left list + right preview) renders correctly within the base-class chrome
- [ ] ls --tui with a directory larger than the terminal height scrolls smoothly without rendering all entries at once
- [ ] Alternating row background and vscroll_indicator are visible
- [ ] Existing keys still work: j/k, Enter, /, o, c, d, q
- [ ] Non-interactive fallback (piped stdout) still falls back to plain ls
