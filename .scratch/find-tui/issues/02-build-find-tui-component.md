# 02 — Build FindTuiComponent

**What to build:** A `FindTuiComponent` class that extends `TuiBase` and renders the `std::vector<FindMatch>` as a scrollable list with a metadata detail footer. The component handles `q` (quit), `j`/`k` (scroll via inherited `handle_nav`), `/` (search/filter via inherited `handle_search`), `n`/`N` (next-prev match), `d` (delete selected), `o` (open in cat TUI), and `p` (preview first 50 lines in detail pane).

**Blocked by:** None — can start immediately (independent from ticket 01; integrates once both are ready).

**Status:** ready-for-agent

- [ ] `FindTuiComponent` class extends `TuiBase`, implements `entries_size()`, `render_row()`, `fill_entries()`, `header_rows()`
- [ ] Single-pane layout: scrollable result list + footer showing selected entry's file type, size, mtime, owner, permissions
- [ ] `d` key calls `unlink`/`rmdir` and removes the entry from the in-memory result list
- [ ] `p` key reads up to 50 lines of the selected file into the detail footer (no syntax highlighting)
- [ ] `o` key exits find TUI and re-enters via cat TUI if stdout is still a TTY; silently no-ops otherwise
- [ ] Component compiles cleanly and links against the existing ftxui dependency
