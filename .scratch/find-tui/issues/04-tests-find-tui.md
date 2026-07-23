# 04 — Tests for find --tui

**What to build:** Integration tests in `tests/test_find.sh` covering the TTY guard fallback, help text, and predicate compatibility with TUI mode. Uses the existing `assert_cmd` / `assert_cmd_pat` / `assert_cmd_not_pat` helpers — no terminal-automation tools needed.

**Blocked by:** 03 — Wire --tui flag + find_tui_main entry point

**Status:** ready-for-agent

- [ ] `find --help` output includes `--tui` flag description
- [ ] `find --tui` piped (non-TTY) produces identical output to `find` (no `--tui`) for same arguments
- [ ] `find --tui -maxdepth 1 -type f -name '*.txt'` piped still respects `-maxdepth`, `-type`, `-name` predicates (fallback parity)
- [ ] All 24 existing `test_find.sh` cases pass unchanged (non-TTY regression guard)
