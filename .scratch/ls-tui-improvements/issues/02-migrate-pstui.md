# 02 — Migrate ps_tui to TuiBase

**What to build:** PsTuiComponent inherits from TuiBase and implements the three virtual hooks. All ps_tui-specific behaviour (process data population, row rendering with CPU/MEM columns, sort/tree/search key bindings) moves into the overrides. After migration, ps_tui must behave identically to before — same header, same process table, same key bindings, same live-refresh behaviour.

**Blocked by:** 01

**Status:** ready-for-agent

- [ ] PsTuiComponent inherits TuiBase, implements fill_entries/render_row/on_command_key
- [ ] ps_tui still sorts by CPU/MEM/PID, toggles tree mode, and refreshes via the async refresher thread
- [ ] Visual regression: ps --tui header, process rows, memory bar, and footer look identical to pre-migration
- [ ] Keyboard regression: j/k, /, q, c/m/p, t, arrow keys, Home/End/PageUp/PageDown all work as before
