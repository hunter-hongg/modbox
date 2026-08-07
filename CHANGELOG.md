CHANGELOG

All notable changes to this project are documented in this file.

## Unreleased

## v0.1.0 (2026-08-04)
-----------------------
- Added `fd` command: recursive file search with regex/glob, filters, color, and exec support. (Implemented in src/commands/fd.cpp)
- Added interactive `ls --tui` and `lf` alias: two-pane file browser using ftxui. (src/commands/ls_tui.cpp, src/commands/lf.cpp)
- Introduced shared TUI base class used by multiple TUIs (include/commands/tui_base.hpp, src/commands/tui_base.cpp).
- Enhanced `cp` with multiple options and correctness improvements: recursive copy, -f, -n, -i, -u, -p, -t, preserve semantics. (src/commands/cp.cpp)
- Added tests and TUI fallbacks; comprehensive test suite passes locally (see tests/run_tests.sh).
- Added GitHub Actions CI workflow: .github/workflows/ci.yml

Notes
-----
See docs/superpowers/STATUS-2026.md for a cross-referenced implementation status and suggested follow-ups (polish, docs reconciliation, changelog tasks).

For contributors
----------------
- Run the test suite locally: bash tests/run_tests.sh
- Build: make
- Add issues for follow-ups or pick items from docs/superpowers/TODOS-2026.md
