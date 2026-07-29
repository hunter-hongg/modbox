Superpowers TODOs — 2026-07-29

These are follow-up tasks identified after marking implemented plans. The repository's session DB refused INSERTs from this agent, so these are written here for tracking and can be imported into any task tracker.

Todos (suggested IDs to use in tracker):

1) run-build-tests-locally
- Title: Run build and full tests locally
- Description: Run `make clean && make compile` then `bash tests/run_tests.sh`. Capture failing tests and logs (tests/*.log). Fix any failures.
- Commands:
  - make clean && make compile
  - bash tests/run_tests.sh | tee tests/full-test.log

2) add-changelog-entry
- Title: Add changelog / release notes
- Description: Create or update CHANGELOG.md (or section in README.md) summarizing implemented features: fd, ls --tui / lf, TuiBase, cp options. Link commits and docs/superpowers/STATUS-2026.md.
- Suggested content: short bullet list + links to STATUS and key source files.

3) polish-ls-tui
- Title: Polish ls --tui preview and UX
- Description: Improve preview pane rendering, icons, keybindings, CWD-on-quit reliability, and add tests for TUI behavior (non-TTY fallback, lf init). Update include/commands/ls_tui.hpp and src/commands/ls_tui.cpp as needed.
- Suggested tests: tests/test_ls_tui.sh, tests/test_lf.sh

4) reconcile-plans-docs
- Title: Reconcile old plans and specs
- Description: Review docs/superpowers/plans/* and specs/*, archive or update C/GLib-centric guidance to reference current C++ implementations. Mark checkboxes implemented and add links to implementing files.

5) add-ci-badge
- Title: Add CI badge to README
- Description: Insert GitHub Actions workflow status badge into README.md linking to the CI workflow (.github/workflows/ci.yml).
- Suggested badge markdown:
  [![CI](https://github.com/hunter-hongg/modbox/actions/workflows/ci.yml/badge.svg)](https://github.com/hunter-hongg/modbox/actions/workflows/ci.yml)

How to import into session todos table (if writable):

Use the session DB or run the following SQL (one INSERT per statement):

INSERT INTO todos (id, title, description, status) VALUES ('run-build-tests-locally', 'Run build and full tests locally', 'Run make clean && make compile then bash tests/run_tests.sh. Capture failing tests and logs (tests/*.log) and fix issues.', 'pending');

(repeat for other rows)

Alternatively, create GitHub issues from these items with the `gh` CLI:

for id in run-build-tests-locally add-changelog-entry polish-ls-tui reconcile-plans-docs add-ci-badge; do gh issue create --title "$id" --body "See docs/superpowers/TODOS-2026.md"; done
