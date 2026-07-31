STATUS — 2026 Superpowers work

Snapshot (2026-07-29): summary of work completed from the 2026 plans/specs and suggested next steps.

Completed (implemented in tree):

- fd command — implemented (src/commands/fd.cpp). Basic walk, glob/regex, color, exec, filters implemented.
- ls --tui (ls_tui) — interactive two-pane TUI implemented (include/commands/ls_tui.hpp, src/commands/ls_tui.cpp).
- lf alias — implemented (src/commands/lf.cpp + include/commands/lf.hpp).
- find-tui — implemented using shared TuiBase (src/commands/find_tui.cpp).
- Shared TUI base (TuiBase) — implemented and used by multiple TUIs (include/commands/tui_base.hpp, src/commands/tui_base.cpp).
- cp enhancements — many options implemented in C++ version (src/commands/cp.cpp): -r, -v, -f, -n (no-clobber), -i (interactive), -u (update), -p (preserve), -t (target-directory). Recursive copy, preserve and update semantics are present.
- Misc TUI commands: cat_tui, grep_tui present and exercising TuiBase (see src/commands/).* and tests.

Notes / Observations:

- Several plan documents under docs/superpowers/plans/ describe tasks that have already been implemented. The plans often reference C-style implementations and step-by-step agent work; the repository now contains C++ implementations and reusable TUI infrastructure.
- The cp plan in docs/superpowers/plans/2026-05-21-cp-options.md is detailed but describes a C/GLib workflow; the actual implementation is in C++ (src/commands/cp.cpp). Consider updating or archiving that plan to avoid duplication.

Suggested next steps (small, actionable):

1. Convert or mark plan files as "Implemented" where appropriate (docs/superpowers/plans/2026-05-21-cp-options.md, docs/superpowers/plans/2026-06-19-fd-command.md, docs/superpowers/plans/2026-07-20-ls-tui.md). Add links to the implementing files and commit.
2. Add short release notes / changelog entry listing these features (README.md or CHANGELOG.md).
3. Run the full test suite and add/enable CI if not present (tests/run_tests.sh should pass locally).
4. Optional: reconcile any remaining plan TODOs (e.g., cp: hardlink/symlink modes if desired) and create concrete issues/todos.

If preferred, update the original plan files in-place (tick checkboxes and add "Implemented: commit/paths") — indicate which plan(s) to edit and a concise format to use, and this will be done.
