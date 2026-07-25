# 01 — Extract shared TuiBase component class

**What to build:** A new abstract `TuiBase` component class that owns all shared TUI scaffolding: scroll offset math (`scroll_offset_`, `max_rows_`, `selected_idx_`), search/filter state (`search_mode_`, `search_input_`, `search_query_`), default key bindings (`j`/`k`, `/`, `q`, arrow keys, Home/End/PageUp/PageDown), header/footer chrome, and alternating row background. Subclasses implement three virtual hooks: `fill_entries()`, `render_row(idx)`, and `on_command_key(event)`. No existing command is migrated yet — the class is added beside the current code and compiles cleanly.

**Blocked by:** None — can start immediately.

**Status:** ready-for-agent

- [ ] TuiBase.hpp declares the abstract base class with all shared state fields and virtual hooks
- [ ] TuiBase.cpp implements default OnRender (header + scrollable body + footer) and default OnEvent (nav keys, search, quit dispatch)
- [ ] Both PsTuiComponent and LsfComponent still compile and run unchanged after the new files land
- [ ] Unit test for TuiBase scroll math: given N entries and max_rows, verify rendered count and offset clamping
