# 02 — Add tests for ls --tui alternating row backgrounds

**What to build:** Automated tests that verify the alternating row background behavior in `ls --tui`. Tests cover parity, color gating, and filter stability.

**Blocked by:** 01 — Add alternating row backgrounds to ls --tui

**Status:** ready-for-agent

- [ ] Test: even-indexed `render_row()` returns a plain text element with no background decoration
- [ ] Test: odd-indexed `render_row()` returns an element with a background color decoration
- [ ] Test: when color is disabled, all rows return plain text elements with no background regardless of index
- [ ] Test: after applying a search filter, the alternating pattern follows visible row index parity (first visible row = even background)
- [ ] Tests use the existing bash test helpers (`assert_cmd`, `assert_cmd_pat`) or the established test seam for TUI components
